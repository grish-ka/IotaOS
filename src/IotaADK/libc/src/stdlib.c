/*
* IotaOS - stdlib.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "stdlib.h"
#include "string.h"
#include "iota.h"
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>

/* Heap: first-fit free list grown with sbrk */
typedef struct block_meta {
    size_t size;
    struct block_meta* next;
    int free;
} block_meta_t;

#define META_SIZE sizeof(struct block_meta)
static block_meta_t* global_base = NULL;

static block_meta_t* find_free_block(block_meta_t** last, size_t size) {
    block_meta_t* current = global_base;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

static block_meta_t* request_space(block_meta_t* last, size_t size) {
    block_meta_t* block = iota_sbrk(0);
    void* request = iota_sbrk(size + META_SIZE);
    if (request == (void*)-1) return NULL;

    if (last) last->next = block;
    block->size = size;
    block->next = NULL;
    block->free = 0;
    return block;
}

void* malloc(size_t size) {
    block_meta_t* block;
    if (size == 0) return NULL;

    /* Align allocations to 8 bytes */
    size = (size + 7) & ~(size_t)7;

    if (!global_base) {
        block = request_space(NULL, size);
        if (!block) return NULL;
        global_base = block;
    } else {
        block_meta_t* last = global_base;
        block = find_free_block(&last, size);
        if (!block) {
            block = request_space(last, size);
            if (!block) return NULL;
        } else {
            block->free = 0;
        }
    }
    return (block + 1);
}

void free(void* ptr) {
    if (!ptr) return;
    block_meta_t* block = ((block_meta_t*)ptr) - 1;
    block->free = 1;
}

void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    if (size != 0 && total / size != num) return NULL;
    void* ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    block_meta_t* block = ((block_meta_t*)ptr) - 1;
    if (block->size >= size) return ptr;

    void* new_ptr = malloc(size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}

void abort(void) {
    iota_syscall3(IOTA_SYS_KILL, iota_getpid(), 6 /* SIGABRT */, 0);
    while(1);
}

#define MAX_ATEXIT 32
static void (*atexit_funcs[MAX_ATEXIT])(void);
static int atexit_count = 0;

int atexit(void (*function)(void)) {
    if (atexit_count >= MAX_ATEXIT) return -1;
    atexit_funcs[atexit_count++] = function;
    return 0;
}

void exit(int status) {
    while (atexit_count > 0) atexit_funcs[--atexit_count]();
    iota_exit(status);
    while(1);
}

/* Environment, filled in by libc_entry from the kernel's NUL-separated block */
#define MAX_ARGS 64
#define MAX_ENV  128
static char* env_table[MAX_ENV];
char** environ = env_table;

char* getenv(const char* name) {
    if (!name || !environ) return NULL;
    size_t len = strlen(name);
    for (int i = 0; environ[i] != NULL; i++) {
        if (strncmp(environ[i], name, len) == 0 && environ[i][len] == '=') {
            return environ[i] + len + 1;
        }
    }
    return NULL;
}

/* The program's name, as glibc provides it (set from argv[0] at start-up) */
char *program_invocation_name = "";
char *program_invocation_short_name = "";
char *__progname = "";

__attribute__((weak)) const char *getprogname(void) {
    return program_invocation_short_name;
}

extern int main(int argc, char* argv[], char* envp[]);

/* The entry point called by crt0.o */
void libc_entry(void) {
    /* Sizes match the kernel's ARGS_SIZE and ENV_SIZE */
    static char raw_args[1024];
    static char raw_env[2048];
    static char* argv[MAX_ARGS + 1];
    int argc = 0;

    /* Arguments arrive as NUL-separated strings ending in an empty one */
    iota_get_args(raw_args);
    char* ptr = raw_args;
    while (*ptr != '\0' && argc < MAX_ARGS) {
        argv[argc++] = ptr;
        ptr += strlen(ptr) + 1;
    }
    if (argc == 0) {
        argv[0] = "";
        argc = 1;
    }
    argv[argc] = NULL;

    program_invocation_name = argv[0];
    program_invocation_short_name = strrchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
    __progname = program_invocation_short_name;

    iota_get_env(raw_env);
    int envc = 0;
    char* eptr = raw_env;
    while (*eptr != '\0' && envc < MAX_ENV - 1) {
        env_table[envc++] = eptr;
        while (*eptr != '\0') eptr++; /* Seek to the NUL of this string */
        eptr++;                       /* Skip to the next string */
    }
    env_table[envc] = NULL;

    exit(main(argc, argv, environ));
}

int atoi(const char *nptr) {
    int res = 0;
    int sign = 1;

    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' || *nptr == '\r') nptr++;

    if (*nptr == '-' || *nptr == '+') {
        if (*nptr == '-') sign = -1;
        nptr++;
    }

    while (*nptr >= '0' && *nptr <= '9') {
        res = res * 10 + (*nptr - '0');
        nptr++;
    }

    return res * sign;
}

intmax_t strtoimax(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    int neg = 0, any = 0, overflow = 0;
    uintmax_t acc = 0;
    uintmax_t limit;

    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    if (*s == '-' || *s == '+') {
        neg = (*s == '-');
        s++;
    }
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        base = 16;
    } else if (base == 0) {
        base = (*s == '0') ? 8 : 10;
    }

    limit = neg ? (uintmax_t)INTMAX_MAX + 1 : (uintmax_t)INTMAX_MAX;
    for (;; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= base) break;
        any = 1;
        if (overflow || acc > (limit - d) / base) {
            overflow = 1;
            continue;
        }
        acc = acc * base + d;
    }

    if (endptr) *endptr = (char *)(any ? s : nptr);
    if (overflow) {
        errno = ERANGE;
        return neg ? INTMAX_MIN : INTMAX_MAX;
    }
    return neg ? (intmax_t)(0 - acc) : (intmax_t)acc;
}

static unsigned long rand_state = 1;

int rand(void) {
    rand_state = rand_state * 1103515245 + 12345;
    return (int)((rand_state / 65536) % ((unsigned long)RAND_MAX + 1));
}

void srand(unsigned int seed) {
    rand_state = seed;
}

int mkstemp(char *template) {
    size_t len = strlen(template);
    if (len < 6 || strcmp(template + len - 6, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }
    char saved[6];
    memcpy(saved, template + len - 6, 6);
    for (int tries = 0; tries < 100; tries++) {
        memcpy(template + len - 6, saved, 6);
        mktemp(template);
        int fd = open(template, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0 || errno != EEXIST) return fd;
    }
    errno = EEXIST;
    return -1;
}

char *mktemp(char *template) {
    static unsigned int counter = 0;
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t len = strlen(template);
    if (len < 6 || strcmp(template + len - 6, "XXXXXX") != 0) {
        errno = EINVAL;
        template[0] = '\0';
        return template;
    }
    unsigned int v = ++counter * 2654435761u ^ (unsigned int)rand();
    for (int i = 1; i <= 6; i++) {
        template[len - i] = chars[v % (sizeof(chars) - 1)];
        v /= (sizeof(chars) - 1);
    }
    return template;
}

/* Bash 5.2 --disable-multibyte parser bug workaround */
static char bash_dummy_property[8192];
char *shell_input_line_property = bash_dummy_property;

static void swap_bytes(unsigned char *a, unsigned char *b, size_t size) {
    while (size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

/* Shell sort: no recursion and no scratch allocation */
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    unsigned char *arr = base;
    size_t gap = 1;
    while (gap < nmemb / 3) gap = gap * 3 + 1;
    for (; gap > 0; gap /= 3) {
        for (size_t i = gap; i < nmemb; i++) {
            for (size_t j = i; j >= gap && compar(arr + (j - gap) * size, arr + j * size) > 0; j -= gap) {
                swap_bytes(arr + (j - gap) * size, arr + j * size, size);
            }
        }
    }
}

/* Environment editing. Strings set here are heap copies; the table grows on demand. */
static int env_capacity = MAX_ENV;

static int env_find(const char *name, size_t len) {
    for (int i = 0; environ[i]; i++) {
        if (strncmp(environ[i], name, len) == 0 && environ[i][len] == '=') return i;
    }
    return -1;
}

static int env_count(void) {
    int n = 0;
    while (environ[n]) n++;
    return n;
}

int putenv(char *string) {
    char *eq = strchr(string, '=');
    if (!eq) return unsetenv(string);
    int i = env_find(string, eq - string);
    if (i >= 0) {
        environ[i] = string;
        return 0;
    }
    int n = env_count();
    if (n + 1 >= env_capacity) {
        char **bigger = malloc(sizeof(char *) * env_capacity * 2);
        if (!bigger) return -1;
        memcpy(bigger, environ, sizeof(char *) * (n + 1));
        environ = bigger;
        env_capacity *= 2;
    }
    environ[n] = string;
    environ[n + 1] = NULL;
    return 0;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !*name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite && getenv(name)) return 0;
    size_t nl = strlen(name), vl = strlen(value);
    char *s = malloc(nl + vl + 2);
    if (!s) return -1;
    memcpy(s, name, nl);
    s[nl] = '=';
    memcpy(s + nl + 1, value, vl + 1);
    return putenv(s);
}

int unsetenv(const char *name) {
    int i = env_find(name, strlen(name));
    if (i < 0) return 0;
    for (; environ[i]; i++) environ[i] = environ[i + 1];
    return 0;
}

int clearenv(void) {
    environ[0] = NULL;
    return 0;
}

long atol(const char *nptr) {
    return strtol(nptr, NULL, 10);
}

long long atoll(const char *nptr) {
    return strtoll(nptr, NULL, 10);
}

int abs(int j) { return j < 0 ? -j : j; }
long labs(long j) { return j < 0 ? -j : j; }
long long llabs(long long j) { return j < 0 ? -j : j; }

/* Parse digits in base; shared by the strto* family */
static unsigned long long parse_unsigned(const char *nptr, char **endptr, int base, int *neg, int *overflow) {
    const char *s = nptr;
    unsigned long long acc = 0;
    int any = 0;

    *neg = 0;
    *overflow = 0;
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    if (*s == '-' || *s == '+') {
        *neg = (*s == '-');
        s++;
    }
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        base = 16;
    } else if (base == 0) {
        base = (*s == '0') ? 8 : 10;
    }
    for (;; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= base) break;
        any = 1;
        if (acc > (~0ULL - d) / base) *overflow = 1;
        else acc = acc * base + d;
    }
    if (endptr) *endptr = (char *)(any ? s : nptr);
    return acc;
}

__attribute__((weak)) long long strtoll(const char *nptr, char **endptr, int base) {
    int neg, overflow;
    unsigned long long v = parse_unsigned(nptr, endptr, base, &neg, &overflow);
    if (overflow || v > (neg ? (unsigned long long)LLONG_MAX + 1 : (unsigned long long)LLONG_MAX)) {
        errno = ERANGE;
        return neg ? LLONG_MIN : LLONG_MAX;
    }
    return neg ? (long long)(0 - v) : (long long)v;
}

__attribute__((weak)) unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    int neg, overflow;
    unsigned long long v = parse_unsigned(nptr, endptr, base, &neg, &overflow);
    if (overflow) {
        errno = ERANGE;
        return ~0ULL;
    }
    return neg ? 0 - v : v;
}

__attribute__((weak)) long strtol(const char *nptr, char **endptr, int base) {
    long long v = strtoll(nptr, endptr, base);
    if (v > LONG_MAX) { errno = ERANGE; return LONG_MAX; }
    if (v < LONG_MIN) { errno = ERANGE; return LONG_MIN; }
    return (long)v;
}

__attribute__((weak)) unsigned long strtoul(const char *nptr, char **endptr, int base) {
    unsigned long long v = strtoull(nptr, endptr, base);
    if (v > ULONG_MAX && v < (0ULL - (unsigned long long)ULONG_MAX)) {
        errno = ERANGE;
        return ULONG_MAX;
    }
    return (unsigned long)v;
}

__attribute__((weak)) double strtod(const char *nptr, char **endptr) {
    const char *s = nptr;
    double v = 0, scale = 1;
    int neg = 0, any = 0;
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    if (*s == '-' || *s == '+') neg = (*s++ == '-');
    for (; *s >= '0' && *s <= '9'; s++, any = 1) v = v * 10 + (*s - '0');
    if (*s == '.') {
        for (s++; *s >= '0' && *s <= '9'; s++, any = 1) {
            scale /= 10;
            v += (*s - '0') * scale;
        }
    }
    if (any && (*s == 'e' || *s == 'E')) {
        const char *e = s + 1;
        int eneg = 0, ev = 0;
        if (*e == '-' || *e == '+') eneg = (*e++ == '-');
        if (*e >= '0' && *e <= '9') {
            for (; *e >= '0' && *e <= '9'; e++) ev = ev * 10 + (*e - '0');
            while (ev--) v = eneg ? v / 10 : v * 10;
            s = e;
        }
    }
    if (endptr) *endptr = (char *)(any ? s : nptr);
    return neg ? -v : v;
}

double atof(const char *nptr) {
    return strtod(nptr, NULL);
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *)) {
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        const void *p = (const char *)base + mid * size;
        int c = compar(key, p);
        if (c == 0) return (void *)p;
        if (c < 0) hi = mid;
        else lo = mid + 1;
    }
    return NULL;
}

int system(const char *command) {
    if (!command) return 1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char *argv[] = {"sh", "-c", (char *)command, NULL};
        execve("/bin/sh", argv, environ);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return status;
}

char *realpath(const char *path, char *resolved) {
    struct stat st;
    if (stat(path, &st) < 0) return NULL;

    char cwd[256];
    char joined[512];
    if (path[0] == '/') {
        strncpy(joined, path, sizeof(joined) - 1);
        joined[sizeof(joined) - 1] = '\0';
    } else {
        getcwd(cwd, sizeof(cwd));
        snprintf(joined, sizeof(joined), "%s/%s", cwd, path);
    }

    /* Canonicalise ".", ".." and repeated slashes */
    char out[256];
    size_t o = 0;
    char *save;
    for (char *seg = strtok_r(joined, "/", &save); seg; seg = strtok_r(NULL, "/", &save)) {
        if (strcmp(seg, ".") == 0) continue;
        if (strcmp(seg, "..") == 0) {
            while (o > 0 && out[o - 1] != '/') o--;
            if (o > 0) o--;
            continue;
        }
        size_t len = strlen(seg);
        if (o + len + 2 > sizeof(out)) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        out[o++] = '/';
        memcpy(out + o, seg, len);
        o += len;
    }
    if (o == 0) out[o++] = '/';
    out[o] = '\0';

    if (!resolved) resolved = malloc(o + 1);
    if (resolved) strcpy(resolved, out);
    return resolved;
}

/* Weak: bash carries its own strtoumax */
__attribute__((weak)) uintmax_t strtoumax(const char *nptr, char **endptr, int base) {
    return strtoull(nptr, endptr, base);
}

/* long double is computed in double precision */
long double strtold(const char *nptr, char **endptr) {
    return strtod(nptr, endptr);
}

float strtof(const char *nptr, char **endptr) {
    return (float)strtod(nptr, endptr);
}
