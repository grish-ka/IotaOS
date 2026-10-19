/*
* IotaOS - scanf.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* Input is either a string or a stream */
typedef struct {
    const char *str;
    FILE *file;
    int peeked;      /* character read ahead from file, or EOF */
    int have_peek;
    long count;      /* characters consumed so far (for %n) */
} source_t;

static int peek(source_t *in) {
    if (in->str) return *in->str ? (unsigned char)*in->str : EOF;
    if (!in->have_peek) {
        in->peeked = fgetc(in->file);
        in->have_peek = 1;
    }
    return in->peeked;
}

static void advance(source_t *in) {
    if (in->str) {
        if (*in->str) in->str++;
    } else {
        in->have_peek = 0;
    }
    in->count++;
}

/* Give back a read-ahead character to a stream when scanning stops */
static void finish(source_t *in) {
    if (in->file && in->have_peek && in->peeked != EOF) ungetc(in->peeked, in->file);
}

static void store_int(va_list *ap, int len, uintmax_t v) {
    switch (len) {
    case 'H': *va_arg(*ap, char *) = (char)v; break;
    case 'h': *va_arg(*ap, short *) = (short)v; break;
    case 'l': *va_arg(*ap, long *) = (long)v; break;
    case 'q': *va_arg(*ap, long long *) = (long long)v; break;
    default: *va_arg(*ap, int *) = (int)v; break;
    }
}

static int digit_value(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return 99;
}

static int vscan(source_t *in, const char *fmt, va_list ap_in) {
    va_list ap;
    va_copy(ap, ap_in);
    int assigned = 0;
    int matched_any = 0;

    for (; *fmt; fmt++) {
        if (isspace((unsigned char)*fmt)) {
            while (isspace(peek(in))) advance(in);
            continue;
        }
        if (*fmt != '%' || fmt[1] == '%') {
            if (*fmt == '%') fmt++;
            if (peek(in) != (unsigned char)*fmt) break;
            advance(in);
            continue;
        }
        fmt++;

        int skip = 0;
        if (*fmt == '*') { skip = 1; fmt++; }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        int len = 0;
        if (*fmt == 'h') { len = 'h'; fmt++; if (*fmt == 'h') { len = 'H'; fmt++; } }
        else if (*fmt == 'l') { len = 'l'; fmt++; if (*fmt == 'l') { len = 'q'; fmt++; } }
        else if (*fmt == 'j' || *fmt == 'L' || *fmt == 'q') { len = 'q'; fmt++; }
        else if (*fmt == 'z' || *fmt == 't') { len = 'l'; fmt++; }

        char conv = *fmt;
        if (conv == '\0') break;
        if (conv == 'n') {
            if (!skip) store_int(&ap, len, in->count);
            continue;
        }
        if (conv != 'c' && conv != '[') {
            while (isspace(peek(in))) advance(in);
        }
        if (peek(in) == EOF) {
            va_end(ap);
            finish(in);
            return (assigned || matched_any) ? assigned : EOF;
        }
        if (width == 0) width = (conv == 'c') ? 1 : 0x7fffffff;

        if (strchr("diouxXp", conv)) {
            int base = conv == 'o' ? 8 : (conv == 'x' || conv == 'X' || conv == 'p') ? 16 : conv == 'i' ? 0 : 10;
            int neg = 0, digits = 0;
            uintmax_t v = 0;
            int c = peek(in);
            if ((c == '-' || c == '+') && width > 0) {
                neg = (c == '-');
                advance(in);
                width--;
            }
            if (width > 0 && peek(in) == '0' && (base == 0 || base == 16)) {
                advance(in);
                width--;
                digits = 1;
                if (width > 0 && (peek(in) == 'x' || peek(in) == 'X')) {
                    advance(in);
                    width--;
                    base = 16;
                    digits = 0;
                } else if (base == 0) {
                    base = 8;
                }
            }
            if (base == 0) base = 10;
            while (width > 0 && digit_value(peek(in)) < base) {
                v = v * base + digit_value(peek(in));
                advance(in);
                width--;
                digits++;
            }
            if (!digits) break;
            if (neg) v = 0 - v;
            if (!skip) {
                if (conv == 'p') *va_arg(ap, void **) = (void *)(uintptr_t)v;
                else store_int(&ap, len, v);
                assigned++;
            }
            matched_any = 1;
        } else if (strchr("feEgGaA", conv)) {
            char buf[64];
            int hexf = (conv == 'a' || conv == 'A');
            int dot = 0, expo = 0;
            int n = 0;
            /* Collect the longest token that could form a number: one dot,
               one e (p for %a) exponent, sign only leading or after it. */
            while (width > 0 && n < (int)sizeof(buf) - 1) {
                int c = peek(in);
                int ok;
                if (hexf ? isxdigit(c) : isdigit(c)) ok = 1;
                else if (hexf && (c == 'x' || c == 'X') && n >= 1 && n <= 2 && buf[n - 1] == '0')
                    ok = 1; /* the 0x / +0x / -0x prefix of a hex float */
                else if (c == '.') ok = !dot && !expo;
                else if (c == '-' || c == '+')
                    ok = n == 0 || (!hexf && (buf[n - 1] == 'e' || buf[n - 1] == 'E')) ||
                         (hexf && n > 0 && (buf[n - 1] == 'p' || buf[n - 1] == 'P'));
                else if (hexf ? (c == 'p' || c == 'P') : (c == 'e' || c == 'E')) ok = !expo;
                else ok = 0;
                if (!ok) break;
                if (c == '.') dot = 1;
                else if (!hexf && (c == 'e' || c == 'E')) expo = 1;
                else if (hexf && (c == 'p' || c == 'P')) expo = 1;
                buf[n++] = (char)c;
                advance(in);
                width--;
            }
            buf[n] = '\0';
            if (n == 0) break;
            /* Whatever strtod did not consume goes back to the input, so
               "1ex" scans 1 and leaves 'e', and "1.2.3" cannot eat ".3". */
            char *endp;
            double d = strtod(buf, &endp);
            int k = (int)(endp - buf);
            if (k < n) {
                in->count -= n - k;
                if (in->str) in->str -= n - k;
                else for (int i = n - 1; i >= k; i--) ungetc((unsigned char)buf[i], in->file);
            }
            if (k == 0) break; /* token held no number: matching failure */
            if (!skip) {
                if (len == 'l' || len == 'q') *va_arg(ap, double *) = d;
                else *va_arg(ap, float *) = (float)d;
                assigned++;
            }
            matched_any = 1;
        } else if (conv == 'c') {
            char *out = skip ? NULL : va_arg(ap, char *);
            int n = 0;
            while (n < width && peek(in) != EOF) {
                if (out) out[n] = (char)peek(in);
                advance(in);
                n++;
            }
            if (n < width) break;
            if (!skip) assigned++;
            matched_any = 1;
        } else if (conv == 's' || conv == '[') {
            unsigned char set[256];
            if (conv == 's') {
                for (int c = 0; c < 256; c++) set[c] = !isspace(c);
            } else {
                fmt++;
                int negate = (*fmt == '^');
                if (negate) fmt++;
                memset(set, 0, sizeof(set));
                int first = 1;
                for (; *fmt && (*fmt != ']' || first); fmt++, first = 0) {
                    if (fmt[1] == '-' && fmt[2] && fmt[2] != ']') {
                        for (int c = (unsigned char)fmt[0]; c <= (unsigned char)fmt[2]; c++) set[c] = 1;
                        fmt += 2;
                    } else {
                        set[(unsigned char)*fmt] = 1;
                    }
                }
                if (!*fmt) break;
                if (negate) for (int c = 0; c < 256; c++) set[c] = !set[c];
            }
            set[0] = 0;
            char *out = skip ? NULL : va_arg(ap, char *);
            int n = 0;
            while (n < width) {
                int c = peek(in);
                if (c == EOF || !set[c]) break;
                if (out) out[n] = (char)c;
                advance(in);
                n++;
            }
            if (n == 0) break;
            if (out) out[n] = '\0';
            if (!skip) assigned++;
            matched_any = 1;
        } else {
            break;
        }
    }
    va_end(ap);
    finish(in);
    return assigned;
}

int vsscanf(const char *str, const char *format, va_list ap) {
    source_t in = { str, NULL, EOF, 0, 0 };
    return vscan(&in, format, ap);
}

int sscanf(const char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vsscanf(str, format, ap);
    va_end(ap);
    return r;
}

int vfscanf(FILE *stream, const char *format, va_list ap) {
    source_t in = { NULL, stream, EOF, 0, 0 };
    return vscan(&in, format, ap);
}

int fscanf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vfscanf(stream, format, ap);
    va_end(ap);
    return r;
}

int vscanf(const char *format, va_list ap) {
    return vfscanf(stdin, format, ap);
}

int scanf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vfscanf(stdin, format, ap);
    va_end(ap);
    return r;
}
