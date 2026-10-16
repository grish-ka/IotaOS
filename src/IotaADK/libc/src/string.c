/* 
 * IotaOS - string.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "string.h"
#include <stdlib.h>

int strcmp(const char* str1, const char* str2) {
    while (*str1 != '\0' && *str1 == *str2) {
        str1++;
        str2++;
    }
    /* The sign matters: sorting and binary searches depend on it */
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* original_dest = dest;
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return original_dest;
}

char* strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);
    while (*src != '\0') {
        *ptr++ = *src++;
    }
    *ptr = '\0';
    return dest;
}

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = ptr;
    while (num--) *p++ = (unsigned char)value;
    return ptr;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        ++s1;
        ++s2;
        --n;
    }
    if (n == 0) return 0;
    else return (*(unsigned char*)s1 - *(unsigned char*)s2);
}

static const char *const error_strings[] = {
    "Success", "Operation not permitted", "No such file or directory", "No such process",
    "Interrupted system call", "Input/output error", "No such device or address",
    "Argument list too long", "Exec format error", "Bad file descriptor", "No child processes",
    "Resource temporarily unavailable", "Cannot allocate memory", "Permission denied",
    "Bad address", "Block device required", "Device or resource busy", "File exists",
    "Invalid cross-device link", "No such device", "Not a directory", "Is a directory",
    "Invalid argument", "Too many open files in system", "Too many open files",
    "Inappropriate ioctl for device", "Text file busy", "File too large",
    "No space left on device", "Illegal seek", "Read-only file system", "Too many links",
    "Broken pipe", "Numerical argument out of domain", "Numerical result out of range",
    "Resource deadlock avoided", "File name too long", "No locks available",
    "Function not implemented", "Directory not empty", "Too many levels of symbolic links",
};

char* strerror(int errnum) {
    if (errnum >= 0 && errnum < (int)(sizeof(error_strings) / sizeof(error_strings[0])))
        return (char*)error_strings[errnum];
    return "Unknown error";
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s != (char)c) {
        if (!*s++) return NULL;
    }
    return (char*)s;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char* h = haystack;
            const char* n = needle;
            while (*h && *n && *h == *n) { 
                h++; 
                n++; 
            }
            if (!*n) return (char*)haystack;
        }
    }
    return NULL;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s) return dest;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for ( ; i < n; i++) dest[i] = '\0';
    return dest;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char *)last;
}

char *strpbrk(const char *s, const char *accept) {
    while (*s) {
        const char *a = accept;
        while (*a) {
            if (*s == *a++)
                return (char *)s;
        }
        s++;
    }
    return NULL;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    for (; n > 0; n--, p++) {
        if (*p == (unsigned char)c) return (void *)p;
    }
    return NULL;
}

int strcoll(const char *s1, const char *s2) {
    /* Only the "C" locale is supported */
    return strcmp(s1, s2);
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = s1, *b = s2;
    for (; n > 0; n--, a++, b++) {
        if (*a != *b) return *a - *b;
    }
    return 0;
}

/* The weak ones below are also provided by bash's lib/sh; its copies win there */

__attribute__((weak)) size_t strnlen(const char *s, size_t maxlen) {
    size_t n = 0;
    while (n < maxlen && s[n]) n++;
    return n;
}

__attribute__((weak)) char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *d = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

char *strndup(const char *s, size_t n) {
    size_t len = strnlen(s, n);
    char *d = malloc(len + 1);
    if (!d) return NULL;
    memcpy(d, s, len);
    d[len] = '\0';
    return d;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *p = dest + strlen(dest);
    while (n-- && *src) *p++ = *src++;
    *p = '\0';
    return dest;
}

size_t strspn(const char *s, const char *accept) {
    size_t n = 0;
    while (s[n] && strchr(accept, s[n])) n++;
    return n;
}

size_t strcspn(const char *s, const char *reject) {
    size_t n = 0;
    while (s[n] && !strchr(reject, s[n])) n++;
    return n;
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    if (!str) str = *saveptr;
    str += strspn(str, delim);
    if (!*str) {
        *saveptr = str;
        return NULL;
    }
    char *end = str + strcspn(str, delim);
    if (*end) *end++ = '\0';
    *saveptr = end;
    return str;
}

char *strtok(char *str, const char *delim) {
    static char *save;
    return strtok_r(str, delim, &save);
}

char *strsep(char **stringp, const char *delim) {
    char *s = *stringp;
    if (!s) return NULL;
    char *end = s + strcspn(s, delim);
    if (*end) {
        *end = '\0';
        *stringp = end + 1;
    } else {
        *stringp = NULL;
    }
    return s;
}

char *strsignal(int sig) {
    static const char *const names[] = {
        "Unknown signal", "Hangup", "Interrupt", "Quit", "Illegal instruction", "Trace/breakpoint trap",
        "Aborted", "Bus error", "Floating point exception", "Killed", "User defined signal 1",
        "Segmentation fault", "User defined signal 2", "Broken pipe", "Alarm clock", "Terminated",
    };
    if (sig > 0 && sig < (int)(sizeof(names) / sizeof(names[0]))) return (char *)names[sig];
    return (char *)names[0];
}

static int lower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

__attribute__((weak)) int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && lower((unsigned char)*s1) == lower((unsigned char)*s2)) {
        s1++;
        s2++;
    }
    return lower((unsigned char)*s1) - lower((unsigned char)*s2);
}

__attribute__((weak)) int strncasecmp(const char *s1, const char *s2, size_t n) {
    for (; n > 0; n--, s1++, s2++) {
        int d = lower((unsigned char)*s1) - lower((unsigned char)*s2);
        if (d || !*s1) return d;
    }
    return 0;
}

__attribute__((weak)) char *strcasestr(const char *haystack, const char *needle) {
    size_t len = strlen(needle);
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, len) == 0) return (char *)haystack;
    }
    return len == 0 ? (char *)haystack : NULL;
}

__attribute__((weak)) char *strchrnul(const char *s, int c) {
    while (*s && *s != (char)c) s++;
    return (char *)s;
}

size_t strxfrm(char *dest, const char *src, size_t n) {
    size_t len = strlen(src);
    if (n > 0) {
        strncpy(dest, src, n - 1);
        dest[n - 1] = '\0';
    }
    return len;
}

void *mempcpy(void *dest, const void *src, size_t n) {
    return (char *)memcpy(dest, src, n) + n;
}

char *stpcpy(char *dest, const char *src) {
    while ((*dest = *src++)) dest++;
    return dest;
}
