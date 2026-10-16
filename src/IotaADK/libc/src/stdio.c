/*
* IotaOS - stdio.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "stdlib.h"
#include "fcntl.h"
#include "iota.h"
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>

/* Streams are unbuffered: every write goes straight to the fd */
#define F_EOF   1
#define F_ERR   2
#define F_STATIC 4

struct _FILE {
    int fd;
    int flags;
    int ungot;  /* pushed-back char from ungetc, or EOF */
};

static FILE std_streams[3] = {
    { 0, F_STATIC, EOF },
    { 1, F_STATIC, EOF },
    { 2, F_STATIC, EOF },
};

FILE *stdin = &std_streams[0];
FILE *stdout = &std_streams[1];
FILE *stderr = &std_streams[2];

/* ------------------------------------------------------------------------ */
/* Formatting engine                                                         */
/* ------------------------------------------------------------------------ */

/* Output sink: fills buf, and calls flush (if any) when it is full */
typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    size_t total;
    int fd;  /* -1 for string output */
} sink_t;

static void sink_flush(sink_t *s) {
    if (s->fd >= 0 && s->len > 0) {
        write(s->fd, s->buf, s->len);
        s->len = 0;
    }
}

static void sink_putc(sink_t *s, char c) {
    if (s->len + 1 >= s->cap && s->fd >= 0) sink_flush(s);
    /* String sinks keep one byte for the terminator */
    if (s->fd >= 0 || s->len + 1 < s->cap) s->buf[s->len++] = c;
    s->total++;
}

static void sink_pad(sink_t *s, char c, int n) {
    while (n-- > 0) sink_putc(s, c);
}

static void sink_puts(sink_t *s, const char *str, size_t n) {
    while (n--) sink_putc(s, *str++);
}

#define FL_LEFT  1
#define FL_PLUS  2
#define FL_SPACE 4
#define FL_ALT   8
#define FL_ZERO  16

/* Emit a formatted number body with sign/prefix, honouring width and padding */
static void emit_field(sink_t *s, const char *prefix, const char *body, size_t body_len,
                       int zeros, int width, int flags) {
    size_t plen = strlen(prefix);
    int pad = width - (int)(plen + zeros + body_len);
    if (!(flags & FL_LEFT) && !(flags & FL_ZERO)) sink_pad(s, ' ', pad);
    sink_puts(s, prefix, plen);
    if (!(flags & FL_LEFT) && (flags & FL_ZERO)) sink_pad(s, '0', pad);
    sink_pad(s, '0', zeros);
    sink_puts(s, body, body_len);
    if (flags & FL_LEFT) sink_pad(s, ' ', pad);
}

static void fmt_integer(sink_t *s, uintmax_t v, int neg, int base, int upper,
                        int width, int prec, int flags, char conv) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[64];
    int n = 0;
    char prefix[4] = "";

    if (prec >= 0) flags &= ~FL_ZERO;
    if (!(v == 0 && prec == 0)) {
        do {
            tmp[sizeof(tmp) - 1 - n++] = digits[v % base];
            v /= base;
        } while (v);
    }

    if (conv == 'd' || conv == 'i') {
        if (neg) strcpy(prefix, "-");
        else if (flags & FL_PLUS) strcpy(prefix, "+");
        else if (flags & FL_SPACE) strcpy(prefix, " ");
    } else if ((flags & FL_ALT) && base == 16 && n > 0) {
        strcpy(prefix, upper ? "0X" : "0x");
    } else if (conv == 'p') {
        strcpy(prefix, "0x");
    }

    int zeros = (prec > n) ? prec - n : 0;
    if ((flags & FL_ALT) && base == 8 && zeros == 0 && (n == 0 || tmp[sizeof(tmp) - n] != '0'))
        zeros = 1;
    emit_field(s, prefix, tmp + sizeof(tmp) - n, n, zeros, width, flags);
}

/* Fixed/exponent float formatting; adequate for printf(1)-style use */
static void fmt_float(sink_t *s, double v, int width, int prec, int flags, char conv) {
    char body[96];
    int n = 0;
    char prefix[2] = "";
    int upper = (conv == 'F' || conv == 'E' || conv == 'G');
    char lc = conv | 0x20;

    if (v != v) { emit_field(s, "", upper ? "NAN" : "nan", 3, 0, width, flags & ~FL_ZERO); return; }
    if (v < 0 || (v == 0 && 1 / v < 0)) { prefix[0] = '-'; v = -v; }
    else if (flags & FL_PLUS) prefix[0] = '+';
    else if (flags & FL_SPACE) prefix[0] = ' ';
    if (v > 1.7976931348623157e308) { emit_field(s, prefix, upper ? "INF" : "inf", 3, 0, width, flags & ~FL_ZERO); return; }

    if (prec < 0) prec = 6;
    if (prec > 30) prec = 30;

    int exp10 = 0;
    int strip = 0;
    if (lc == 'g') {
        double t = v;
        if (prec == 0) prec = 1;
        if (t != 0) {
            while (t >= 10) { t /= 10; exp10++; }
            while (t < 1) { t *= 10; exp10--; }
        }
        if (exp10 < -4 || exp10 >= prec) { lc = 'e'; prec -= 1; }
        else { lc = 'f'; prec = prec - 1 - exp10; }
        strip = !(flags & FL_ALT);
        exp10 = 0;
    }
    if (lc == 'e' && v != 0) {
        while (v >= 10) { v /= 10; exp10++; }
        while (v < 1) { v *= 10; exp10--; }
    }

    /* Round at the requested precision */
    double r = 0.5;
    for (int i = 0; i < prec; i++) r /= 10;
    v += r;
    if (lc == 'e' && v >= 10) { v /= 10; exp10++; }

    /* Integer part */
    int in = 0;  /* guards against runaway magnitudes */
    double ip = 1;
    while (ip * 10 <= v && in < 40) { ip *= 10; in++; }
    for (; ip >= 1; ip /= 10) {
        int d = (int)(v / ip);
        if (d > 9) d = 9;
        body[n++] = '0' + d;
        v -= d * ip;
    }

    if (prec > 0 || (flags & FL_ALT)) body[n++] = '.';
    for (int i = 0; i < prec; i++) {
        v *= 10;
        int d = (int)v;
        if (d > 9) d = 9;
        body[n++] = '0' + d;
        v -= d;
    }
    if (strip) {
        int dot = -1;
        for (int i = 0; i < n; i++) if (body[i] == '.') dot = i;
        if (dot >= 0) {
            while (n > dot + 1 && body[n - 1] == '0') n--;
            if (n == dot + 1) n--;
        }
    }
    if (lc == 'e') {
        body[n++] = upper ? 'E' : 'e';
        body[n++] = exp10 < 0 ? '-' : '+';
        int e = exp10 < 0 ? -exp10 : exp10;
        if (e >= 100) body[n++] = '0' + e / 100;
        body[n++] = '0' + (e / 10) % 10;
        body[n++] = '0' + e % 10;
    }
    emit_field(s, prefix, body, n, 0, width, flags);
}

static void format(sink_t *s, const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            sink_putc(s, *fmt);
            continue;
        }
        fmt++;

        int flags = 0;
        for (;; fmt++) {
            if (*fmt == '-') flags |= FL_LEFT;
            else if (*fmt == '+') flags |= FL_PLUS;
            else if (*fmt == ' ') flags |= FL_SPACE;
            else if (*fmt == '#') flags |= FL_ALT;
            else if (*fmt == '0') flags |= FL_ZERO;
            else break;
        }
        if (flags & FL_LEFT) flags &= ~FL_ZERO;

        int width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) { flags |= FL_LEFT; flags &= ~FL_ZERO; width = -width; }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        }

        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                if (prec < 0) prec = -1;
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0');
            }
        }

        /* Length modifier: 0=int 1=long 2=long long 3=short 4=char 5=intmax/size/ptrdiff */
        int len = 0;
        int long_double = 0;
        switch (*fmt) {
        case 'h': fmt++; len = 3; if (*fmt == 'h') { fmt++; len = 4; } break;
        case 'l': fmt++; len = 1; if (*fmt == 'l') { fmt++; len = 2; } break;
        case 'q': fmt++; len = 2; break;
        case 'j': fmt++; len = 2; break;
        case 'z': case 't': fmt++; len = 1; break;
        case 'L': fmt++; long_double = 1; break;
        }

        char conv = *fmt;
        switch (conv) {
        case 'd': case 'i': {
            intmax_t v;
            if (len == 2) v = va_arg(ap, long long);
            else if (len == 1) v = va_arg(ap, long);
            else v = va_arg(ap, int);
            if (len == 3) v = (short)v;
            else if (len == 4) v = (signed char)v;
            uintmax_t uv = v < 0 ? (uintmax_t)0 - (uintmax_t)v : (uintmax_t)v;
            fmt_integer(s, uv, v < 0, 10, 0, width, prec, flags, conv);
            break;
        }
        case 'u': case 'o': case 'x': case 'X': {
            uintmax_t v;
            if (len == 2) v = va_arg(ap, unsigned long long);
            else if (len == 1) v = va_arg(ap, unsigned long);
            else v = va_arg(ap, unsigned int);
            if (len == 3) v = (unsigned short)v;
            else if (len == 4) v = (unsigned char)v;
            int base = conv == 'u' ? 10 : conv == 'o' ? 8 : 16;
            fmt_integer(s, v, 0, base, conv == 'X', width, prec, flags, conv);
            break;
        }
        case 'p':
            fmt_integer(s, (uintptr_t)va_arg(ap, void *), 0, 16, 0, width, prec, flags & ~FL_ALT, 'p');
            break;
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (!(flags & FL_LEFT)) sink_pad(s, ' ', width - 1);
            sink_putc(s, c);
            if (flags & FL_LEFT) sink_pad(s, ' ', width - 1);
            break;
        }
        case 's': {
            const char *str = va_arg(ap, const char *);
            if (!str) str = "(null)";
            size_t n = 0;
            while (str[n] && (prec < 0 || n < (size_t)prec)) n++;
            if (!(flags & FL_LEFT)) sink_pad(s, ' ', width - (int)n);
            sink_puts(s, str, n);
            if (flags & FL_LEFT) sink_pad(s, ' ', width - (int)n);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
            double v = long_double ? (double)va_arg(ap, long double) : va_arg(ap, double);
            fmt_float(s, v, width, prec, flags, conv);
            break;
        }
        case 'n':
            if (len == 2) *va_arg(ap, long long *) = s->total;
            else if (len == 1) *va_arg(ap, long *) = s->total;
            else *va_arg(ap, int *) = s->total;
            break;
        case '%':
            sink_putc(s, '%');
            break;
        case '\0':
            return;
        default:
            /* Unknown conversion: print it verbatim */
            sink_putc(s, '%');
            sink_putc(s, conv);
            break;
        }
    }
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    char dummy;
    sink_t s = { size ? str : &dummy, size ? size : 1, 0, 0, -1 };
    format(&s, fmt, ap);
    s.buf[s.len] = '\0';
    return (int)s.total;
}

int vsprintf(char *str, const char *fmt, va_list ap) {
    return vsnprintf(str, (size_t)0x7fffffff, fmt, ap);
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(str, fmt, ap);
    va_end(ap);
    return r;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
    char buf[256];
    sink_t s = { buf, sizeof(buf), 0, 0, stream->fd };
    format(&s, fmt, ap);
    sink_flush(&s);
    return (int)s.total;
}

int vprintf(const char *fmt, va_list ap) {
    return vfprintf(stdout, fmt, ap);
}

int fprintf(FILE *stream, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stream, fmt, ap);
    va_end(ap);
    return r;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return r;
}

/* ------------------------------------------------------------------------ */
/* Streams                                                                   */
/* ------------------------------------------------------------------------ */

FILE *fdopen(int fd, const char *mode) {
    (void)mode;
    if (fd < 0) return NULL;
    FILE *f = malloc(sizeof(FILE));
    if (!f) return NULL;
    f->fd = fd;
    f->flags = 0;
    f->ungot = EOF;
    return f;
}

FILE *fopen(const char *pathname, const char *mode) {
    int oflag = O_RDONLY;
    if (mode[0] == 'w') oflag = O_WRONLY | O_CREAT | O_TRUNC;
    else if (mode[0] == 'a') oflag = O_WRONLY | O_CREAT | O_APPEND;
    if (strchr(mode, '+')) oflag = (oflag & ~(O_RDONLY | O_WRONLY)) | O_RDWR;

    int fd = open(pathname, oflag, 0666);
    if (fd < 0) return NULL;
    FILE *f = fdopen(fd, mode);
    if (!f) close(fd);
    return f;
}

int fclose(FILE *stream) {
    if (!stream) return EOF;
    int r = close(stream->fd);
    if (!(stream->flags & F_STATIC)) free(stream);
    return r < 0 ? EOF : 0;
}

int fileno(FILE *stream) {
    return stream->fd;
}

int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

int feof(FILE *stream) { return (stream->flags & F_EOF) != 0; }
int ferror(FILE *stream) { return (stream->flags & F_ERR) != 0; }
void clearerr(FILE *stream) { stream->flags &= ~(F_EOF | F_ERR); }

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t bytes = size * nmemb;
    if (bytes == 0) return 0;
    ssize_t w = write(stream->fd, ptr, bytes);
    if (w < 0) {
        stream->flags |= F_ERR;
        return 0;
    }
    return (size_t)w / size;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t bytes = size * nmemb;
    size_t got = 0;
    unsigned char *p = ptr;
    if (bytes == 0) return 0;
    if (stream->ungot != EOF) {
        *p++ = (unsigned char)stream->ungot;
        stream->ungot = EOF;
        got = 1;
    }
    while (got < bytes) {
        ssize_t r = read(stream->fd, p, bytes - got);
        if (r < 0) { stream->flags |= F_ERR; break; }
        if (r == 0) { stream->flags |= F_EOF; break; }
        got += r;
        p += r;
    }
    return got / size;
}

int fgetc(FILE *stream) {
    unsigned char c;
    if (stream->ungot != EOF) {
        c = (unsigned char)stream->ungot;
        stream->ungot = EOF;
        return c;
    }
    ssize_t r = read(stream->fd, &c, 1);
    if (r <= 0) {
        stream->flags |= (r == 0) ? F_EOF : F_ERR;
        return EOF;
    }
    return c;
}

int getc(FILE *stream) { return fgetc(stream); }
int getchar(void) { return fgetc(stdin); }

int ungetc(int c, FILE *stream) {
    if (c == EOF || stream->ungot != EOF) return EOF;
    stream->ungot = (unsigned char)c;
    stream->flags &= ~F_EOF;
    return (unsigned char)c;
}

char *fgets(char *s, int size, FILE *stream) {
    int i = 0;
    if (size <= 0) return NULL;
    while (i < size - 1) {
        int c = fgetc(stream);
        if (c == EOF) break;
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    if (i == 0) return NULL;
    s[i] = '\0';
    return s;
}

/* Legacy IotaOS API: reads one line (up to max_size-1 bytes) from the console,
   without the trailing newline */
char *gets(char *buffer, int max_size) {
    int n = read(0, buffer, max_size > 0 ? max_size - 1 : 0);
    if (n < 0) n = 0;
    if (n > 0 && buffer[n - 1] == '\n') n--;
    buffer[n] = '\0';
    return buffer;
}

int fputc(int c, FILE *stream) {
    unsigned char ch = (unsigned char)c;
    if (write(stream->fd, &ch, 1) != 1) {
        stream->flags |= F_ERR;
        return EOF;
    }
    return ch;
}

int putc(int c, FILE *stream) { return fputc(c, stream); }
int putchar(int c) { return fputc(c, stdout); }

int fputs(const char *s, FILE *stream) {
    size_t n = strlen(s);
    if (n && write(stream->fd, s, n) < 0) {
        stream->flags |= F_ERR;
        return EOF;
    }
    return 0;
}

int puts(const char *s) {
    if (fputs(s, stdout) == EOF) return EOF;
    return fputc('\n', stdout) == EOF ? EOF : 0;
}

int fscanf(FILE *stream, const char *format, ...) {
    (void)stream;
    (void)format;
    return EOF;
}

int rename(const char *oldpath, const char *newpath) {
    int r = iota_syscall3(IOTA_SYS_RENAME, (uint32_t)oldpath, (uint32_t)newpath, 0);
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return 0;
}

void perror(const char *s) {
    if (s && *s) fprintf(stderr, "%s: %s\n", s, strerror(errno));
    else fprintf(stderr, "%s\n", strerror(errno));
}

int remove(const char *pathname) {
    struct stat st;
    if (stat(pathname, &st) == 0 && S_ISDIR(st.st_mode)) return rmdir(pathname);
    return unlink(pathname);
}

/* Streams are unbuffered, so buffering requests are accepted and ignored */
int setvbuf(FILE *stream, char *buf, int mode, size_t size) {
    (void)stream; (void)buf; (void)mode; (void)size;
    return 0;
}

void setbuf(FILE *stream, char *buf) {
    (void)stream; (void)buf;
}

void setlinebuf(FILE *stream) {
    (void)stream;
}

int fseek(FILE *stream, long offset, int whence) {
    stream->ungot = EOF;
    stream->flags &= ~F_EOF;
    return lseek(stream->fd, offset, whence) < 0 ? -1 : 0;
}

long ftell(FILE *stream) {
    long pos = lseek(stream->fd, 0, SEEK_CUR);
    if (pos >= 0 && stream->ungot != EOF) pos--;
    return pos;
}

void rewind(FILE *stream) {
    fseek(stream, 0, SEEK_SET);
    clearerr(stream);
}

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream) {
    if (!*lineptr || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (!*lineptr) return -1;
    }
    size_t len = 0;
    int c;
    while ((c = fgetc(stream)) != EOF) {
        if (len + 2 > *n) {
            char *bigger = realloc(*lineptr, *n * 2);
            if (!bigger) return -1;
            *lineptr = bigger;
            *n *= 2;
        }
        (*lineptr)[len++] = (char)c;
        if (c == delim) break;
    }
    if (len == 0) return -1;
    (*lineptr)[len] = '\0';
    return len;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    return getdelim(lineptr, n, '\n', stream);
}

/* Weak: bash carries its own dprintf */
__attribute__((weak)) int dprintf(int fd, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write(fd, buf, n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1);
    return n;
}
