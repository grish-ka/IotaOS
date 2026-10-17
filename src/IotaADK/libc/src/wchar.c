/*
* IotaOS - wchar.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <wchar.h>
#include <stdio.h>
#include <wctype.h>
#include <langinfo.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* ---- conversions: one byte is one character ---- */

size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) {
    (void)ps;
    if (!s) return 0;
    if (n == 0) return (size_t)-2;
    wchar_t c = (unsigned char)*s;
    if (pwc) *pwc = c;
    return c ? 1 : 0;
}

size_t mbrlen(const char *s, size_t n, mbstate_t *ps) {
    return mbrtowc(NULL, s, n, ps);
}

size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps) {
    (void)ps;
    if (!s) return 1;
    if (wc < 0 || wc > 255) {
        errno = EILSEQ;
        return (size_t)-1;
    }
    *s = (char)wc;
    return 1;
}

int mbsinit(const mbstate_t *ps) {
    (void)ps;
    return 1;
}

size_t mbsnrtowcs(wchar_t *dst, const char **src, size_t nms, size_t len, mbstate_t *ps) {
    (void)ps;
    const char *s = *src;
    size_t i = 0;
    for (; i < nms && (!dst || i < len); i++) {
        wchar_t c = (unsigned char)s[i];
        if (dst) dst[i] = c;
        if (!c) {
            if (dst) *src = NULL;
            return i;
        }
    }
    if (dst) *src = s + i;
    return i;
}

size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps) {
    return mbsnrtowcs(dst, src, (size_t)-1, len, ps);
}

size_t wcsnrtombs(char *dst, const wchar_t **src, size_t nwc, size_t len, mbstate_t *ps) {
    (void)ps;
    const wchar_t *s = *src;
    size_t i = 0;
    for (; i < nwc && (!dst || i < len); i++) {
        wchar_t c = s[i];
        if (c < 0 || c > 255) {
            errno = EILSEQ;
            return (size_t)-1;
        }
        if (dst) dst[i] = (char)c;
        if (!c) {
            if (dst) *src = NULL;
            return i;
        }
    }
    if (dst) *src = s + i;
    return i;
}

size_t wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps) {
    return wcsnrtombs(dst, src, (size_t)-1, len, ps);
}

wint_t btowc(int c) {
    return (c == EOF) ? WEOF : (wint_t)(unsigned char)c;
}

int wctob(wint_t c) {
    return (c <= 255) ? (int)c : EOF;
}

int mbtowc(wchar_t *pwc, const char *s, size_t n) {
    if (!s) return 0;
    return (int)mbrtowc(pwc, s, n, NULL);
}

int wctomb(char *s, wchar_t wc) {
    if (!s) return 0;
    return (int)wcrtomb(s, wc, NULL);
}

int mblen(const char *s, size_t n) {
    return mbtowc(NULL, s, n);
}

size_t mbstowcs(wchar_t *dst, const char *src, size_t len) {
    return mbsrtowcs(dst, &src, len, NULL);
}

size_t wcstombs(char *dst, const wchar_t *src, size_t len) {
    return wcsrtombs(dst, &src, len, NULL);
}

/* ---- wide strings ---- */

size_t wcslen(const wchar_t *s) { size_t n = 0; while (s[n]) n++; return n; }
size_t wcsnlen(const wchar_t *s, size_t m) { size_t n = 0; while (n < m && s[n]) n++; return n; }
wchar_t *wcscpy(wchar_t *d, const wchar_t *s) { wchar_t *r = d; while ((*d++ = *s++)) {} return r; }
wchar_t *wcsncpy(wchar_t *d, const wchar_t *s, size_t n) {
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}
wchar_t *wcscat(wchar_t *d, const wchar_t *s) { wcscpy(d + wcslen(d), s); return d; }
wchar_t *wcsncat(wchar_t *d, const wchar_t *s, size_t n) {
    wchar_t *p = d + wcslen(d);
    while (n-- && *s) *p++ = *s++;
    *p = 0;
    return d;
}
int wcscmp(const wchar_t *a, const wchar_t *b) {
    while (*a && *a == *b) { a++; b++; }
    return (*a > *b) - (*a < *b);
}
int wcsncmp(const wchar_t *a, const wchar_t *b, size_t n) {
    for (; n; n--, a++, b++) {
        if (*a != *b) return (*a > *b) - (*a < *b);
        if (!*a) return 0;
    }
    return 0;
}
int wcscoll(const wchar_t *a, const wchar_t *b) { return wcscmp(a, b); }
size_t wcsxfrm(wchar_t *d, const wchar_t *s, size_t n) {
    size_t len = wcslen(s);
    if (n) wcsncpy(d, s, n);
    return len;
}
wchar_t *wcschr(const wchar_t *s, wchar_t c) {
    for (;; s++) {
        if (*s == c) return (wchar_t *)s;
        if (!*s) return NULL;
    }
}
wchar_t *wcsrchr(const wchar_t *s, wchar_t c) {
    const wchar_t *last = NULL;
    for (;; s++) {
        if (*s == c) last = s;
        if (!*s) return (wchar_t *)last;
    }
}
wchar_t *wcsstr(const wchar_t *h, const wchar_t *n) {
    size_t len = wcslen(n);
    for (; *h; h++) if (!wcsncmp(h, n, len)) return (wchar_t *)h;
    return len ? NULL : (wchar_t *)h;
}
wchar_t *wcsdup(const wchar_t *s) {
    size_t len = wcslen(s) + 1;
    wchar_t *d = malloc(len * sizeof(wchar_t));
    if (d) wmemcpy(d, s, len);
    return d;
}
size_t wcsspn(const wchar_t *s, const wchar_t *a) { size_t n = 0; while (s[n] && wcschr(a, s[n])) n++; return n; }
size_t wcscspn(const wchar_t *s, const wchar_t *r) { size_t n = 0; while (s[n] && !wcschr(r, s[n])) n++; return n; }
wchar_t *wcspbrk(const wchar_t *s, const wchar_t *a) { s += wcscspn(s, a); return *s ? (wchar_t *)s : NULL; }

/* Wide strings are converted to bytes and parsed by the narrow versions */
long wcstol(const wchar_t *s, wchar_t **end, int base) {
    char buf[64];
    size_t i = 0;
    for (; s[i] && s[i] < 256 && i < sizeof(buf) - 1; i++) buf[i] = (char)s[i];
    buf[i] = 0;
    char *e;
    long v = strtol(buf, &e, base);
    if (end) *end = (wchar_t *)s + (e - buf);
    return v;
}
unsigned long wcstoul(const wchar_t *s, wchar_t **end, int base) {
    char buf[64];
    size_t i = 0;
    for (; s[i] && s[i] < 256 && i < sizeof(buf) - 1; i++) buf[i] = (char)s[i];
    buf[i] = 0;
    char *e;
    unsigned long v = strtoul(buf, &e, base);
    if (end) *end = (wchar_t *)s + (e - buf);
    return v;
}

wchar_t *wmemcpy(wchar_t *d, const wchar_t *s, size_t n) { return memcpy(d, s, n * sizeof(wchar_t)); }
wchar_t *wmemmove(wchar_t *d, const wchar_t *s, size_t n) { return memmove(d, s, n * sizeof(wchar_t)); }
wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n) { for (size_t i = 0; i < n; i++) s[i] = c; return s; }
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n) {
    for (; n; n--, s++) if (*s == c) return (wchar_t *)s;
    return NULL;
}
int wmemcmp(const wchar_t *a, const wchar_t *b, size_t n) {
    for (; n; n--, a++, b++) if (*a != *b) return (*a > *b) - (*a < *b);
    return 0;
}

/* ---- display width ---- */

int wcwidth(wchar_t c) {
    if (c == 0) return 0;
    if (c < 32 || (c >= 0x7f && c < 0xa0)) return -1;
    return 1;
}

int wcswidth(const wchar_t *s, size_t n) {
    int w = 0;
    for (; n && *s; n--, s++) {
        int cw = wcwidth(*s);
        if (cw < 0) return -1;
        w += cw;
    }
    return w;
}

/* ---- classification: ASCII rules, other characters are "printable" ---- */

#define ASCII(c) ((c) < 128)
int iswalnum(wint_t c) { return ASCII(c) && isalnum((int)c); }
int iswalpha(wint_t c) { return ASCII(c) && isalpha((int)c); }
int iswblank(wint_t c) { return c == ' ' || c == '\t'; }
int iswcntrl(wint_t c) { return c < 32 || c == 127 || (c >= 0x80 && c < 0xa0); }
int iswdigit(wint_t c) { return c >= '0' && c <= '9'; }
int iswgraph(wint_t c) { return ASCII(c) ? isgraph((int)c) : (c >= 0xa1 && c != WEOF); }
int iswlower(wint_t c) { return ASCII(c) && islower((int)c); }
int iswprint(wint_t c) { return ASCII(c) ? isprint((int)c) : (c >= 0xa0 && c != WEOF); }
int iswpunct(wint_t c) { return ASCII(c) && ispunct((int)c); }
int iswspace(wint_t c) { return ASCII(c) && isspace((int)c); }
int iswupper(wint_t c) { return ASCII(c) && isupper((int)c); }
int iswxdigit(wint_t c) { return ASCII(c) && isxdigit((int)c); }
wint_t towlower(wint_t c) { return ASCII(c) ? (wint_t)tolower((int)c) : c; }
wint_t towupper(wint_t c) { return ASCII(c) ? (wint_t)toupper((int)c) : c; }

static const char *const class_names[] = {
    "alnum", "alpha", "blank", "cntrl", "digit", "graph",
    "lower", "print", "punct", "space", "upper", "xdigit",
};
static int (*const class_funcs[])(wint_t) = {
    iswalnum, iswalpha, iswblank, iswcntrl, iswdigit, iswgraph,
    iswlower, iswprint, iswpunct, iswspace, iswupper, iswxdigit,
};

wctype_t wctype(const char *name) {
    for (int i = 0; i < 12; i++) if (!strcmp(name, class_names[i])) return i + 1;
    return 0;
}

int iswctype(wint_t c, wctype_t type) {
    return (type >= 1 && type <= 12) ? class_funcs[type - 1](c) : 0;
}

wctrans_t wctrans(const char *name) {
    if (!strcmp(name, "tolower")) return 1;
    if (!strcmp(name, "toupper")) return 2;
    return 0;
}

wint_t towctrans(wint_t c, wctrans_t t) {
    return t == 1 ? towlower(c) : t == 2 ? towupper(c) : c;
}

/* ---- wide stdio: bytes in, bytes out ---- */

wint_t fgetwc(FILE *f) { int c = fgetc(f); return c == EOF ? WEOF : (wint_t)c; }
wint_t getwc(FILE *f) { return fgetwc(f); }
wint_t getwchar(void) { return fgetwc(stdin); }
wint_t fputwc(wchar_t c, FILE *f) { return fputc(c, f) == EOF ? WEOF : (wint_t)c; }
wint_t putwc(wchar_t c, FILE *f) { return fputwc(c, f); }
wint_t putwchar(wchar_t c) { return fputwc(c, stdout); }
wint_t ungetwc(wint_t c, FILE *f) { return ungetc(c == WEOF ? EOF : (int)c, f) == EOF ? WEOF : c; }
int fputws(const wchar_t *s, FILE *f) {
    for (; *s; s++) if (fputwc(*s, f) == WEOF) return -1;
    return 0;
}
int fwide(FILE *f, int mode) { (void)f; return mode; }

/* ---- locale information for the "C" locale ---- */

static const char *const abday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *const day[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *const abmon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char *const mon[] = {"January", "February", "March", "April", "May", "June", "July",
                                  "August", "September", "October", "November", "December"};

char *nl_langinfo(nl_item item) {
    if (item >= ABDAY_1 && item < ABDAY_1 + 7) return (char *)abday[item - ABDAY_1];
    if (item >= DAY_1 && item < DAY_1 + 7) return (char *)day[item - DAY_1];
    if (item >= ABMON_1 && item < ABMON_1 + 12) return (char *)abmon[item - ABMON_1];
    if (item >= MON_1 && item < MON_1 + 12) return (char *)mon[item - MON_1];
    switch (item) {
    case CODESET: return "ANSI_X3.4-1968";
    case RADIXCHAR: return ".";
    case THOUSEP: return "";
    case D_T_FMT: return "%a %b %e %H:%M:%S %Y";
    case D_FMT: return "%m/%d/%y";
    case T_FMT: return "%H:%M:%S";
    case T_FMT_AMPM: return "%I:%M:%S %p";
    case AM_STR: return "AM";
    case PM_STR: return "PM";
    case YESEXPR: return "^[yY]";
    case NOEXPR: return "^[nN]";
    }
    return "";
}
