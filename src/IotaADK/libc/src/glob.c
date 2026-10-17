/*
* IotaOS - glob.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <glob.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Shell wildcard match for one path component: * ? [set] [!set] \x */
static int match(const char *p, const char *s, int noescape) {
    for (; *p; p++, s++) {
        if (*p == '*') {
            while (p[1] == '*') p++;
            for (const char *t = s;; t++) {
                if (match(p + 1, t, noescape)) return 1;
                if (!*t) return 0;
            }
        }
        if (!*s) return 0;
        if (*p == '?') continue;
        if (*p == '[') {
            const char *q = p + 1;
            int neg = (*q == '!' || *q == '^');
            if (neg) q++;
            int hit = 0, first = 1;
            for (; *q && (*q != ']' || first); q++, first = 0) {
                if (q[1] == '-' && q[2] && q[2] != ']') {
                    if (*s >= q[0] && *s <= q[2]) hit = 1;
                    q += 2;
                } else if (*q == *s) {
                    hit = 1;
                }
            }
            if (!*q) {               /* no closing ']': a literal '[' */
                if (*s != '[') return 0;
                continue;
            }
            if (hit == neg) return 0;
            p = q;
            continue;
        }
        if (*p == '\\' && !noescape && p[1]) p++;
        if (*p != *s) return 0;
    }
    return *s == '\0';
}

static int has_magic(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '*' || s[i] == '?' || s[i] == '[') return 1;
    }
    return 0;
}

typedef struct {
    char **v;
    size_t n, cap;
} list_t;

static int add(list_t *l, const char *path) {
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        char **bigger = realloc(l->v, sizeof(char *) * l->cap);
        if (!bigger) return -1;
        l->v = bigger;
    }
    l->v[l->n] = strdup(path);
    return l->v[l->n++] ? 0 : -1;
}

/* Expand rest (a pattern) below the directory prefix */
static int expand(const char *prefix, const char *rest, int flags, list_t *out) {
    while (*rest == '/') rest++;
    if (!*rest) {
        struct stat st;
        if (stat(prefix[0] ? prefix : "/", &st) < 0) return 0;
        if ((flags & GLOB_MARK) && S_ISDIR(st.st_mode)) {
            char marked[1024];
            size_t len = strlen(prefix);
            if (len + 2 > sizeof(marked)) return 0;
            memcpy(marked, prefix, len);
            if (len == 0 || marked[len - 1] != '/') marked[len++] = '/';
            marked[len] = '\0';
            return add(out, marked);
        }
        return add(out, prefix);
    }

    const char *slash = strchr(rest, '/');
    size_t clen = slash ? (size_t)(slash - rest) : strlen(rest);
    char comp[256];
    if (clen >= sizeof(comp)) return 0;
    memcpy(comp, rest, clen);
    comp[clen] = '\0';
    const char *after = rest + clen;

    char path[1024];
    if (!has_magic(comp, clen)) {
        /* Literal component: just descend */
        if (strlen(prefix) + clen + 2 > sizeof(path)) return 0;
        strcpy(path, prefix);
        if (!(path[0] == '/' && path[1] == '\0')) strcat(path, "/");
        if (prefix[0] == '\0') path[0] = '\0';
        strcat(path, comp);
        if (!*after || *after == '/') {
            struct stat st;
            if (stat(path, &st) < 0) return 0;
        }
        return expand(path, after, flags, out);
    }

    DIR *d = opendir(prefix[0] ? prefix : ".");
    if (!d) return 0;
    struct dirent *de;
    int r = 0;
    while ((de = readdir(d)) != NULL && r == 0) {
        if (de->d_name[0] == '.' && comp[0] != '.') continue;
        if (!match(comp, de->d_name, flags & GLOB_NOESCAPE)) continue;
        if (strlen(prefix) + strlen(de->d_name) + 2 > sizeof(path)) continue;
        if (prefix[0] == '\0') strcpy(path, de->d_name);
        else if (prefix[0] == '/' && prefix[1] == '\0') { strcpy(path, "/"); strcat(path, de->d_name); }
        else { strcpy(path, prefix); strcat(path, "/"); strcat(path, de->d_name); }
        r = expand(path, after, flags, out);
    }
    closedir(d);
    return r;
}

static int compare(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

int glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob) {
    (void)errfunc;
    list_t found = {0};
    const char *rest = pattern;
    const char *prefix = "";
    if (*pattern == '/') {
        prefix = "/";
        rest = pattern + 1;
    }
    if (expand(prefix, rest, flags, &found) < 0) return GLOB_NOSPACE;

    if (found.n == 0) {
        if (!(flags & GLOB_NOCHECK)) {
            if (!(flags & GLOB_APPEND)) {
                pglob->gl_pathc = 0;
                pglob->gl_pathv = NULL;
            }
            return GLOB_NOMATCH;
        }
        add(&found, pattern);
    }
    if (!(flags & GLOB_NOSORT)) qsort(found.v, found.n, sizeof(char *), compare);

    size_t offs = (flags & GLOB_DOOFFS) ? pglob->gl_offs : 0;
    size_t old = (flags & GLOB_APPEND) ? pglob->gl_pathc : 0;
    char **v = realloc((flags & GLOB_APPEND) ? pglob->gl_pathv : NULL,
                       sizeof(char *) * (offs + old + found.n + 1));
    if (!v) return GLOB_NOSPACE;
    if (!(flags & GLOB_APPEND)) {
        for (size_t i = 0; i < offs; i++) v[i] = NULL;
    }
    for (size_t i = 0; i < found.n; i++) v[offs + old + i] = found.v[i];
    v[offs + old + found.n] = NULL;
    free(found.v);
    pglob->gl_pathv = v;
    pglob->gl_pathc = old + found.n;
    return 0;
}

void globfree(glob_t *pglob) {
    if (!pglob->gl_pathv) return;
    for (size_t i = 0; i < pglob->gl_pathc; i++) free(pglob->gl_pathv[pglob->gl_offs + i]);
    free(pglob->gl_pathv);
    pglob->gl_pathv = NULL;
    pglob->gl_pathc = 0;
}
