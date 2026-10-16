/*
* IotaOS - find.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <dirent.h>
#include <sys/stat.h>

/* find [PATH...] [-name GLOB] [-iname GLOB] [-type f|d|c] [-maxdepth N] [-mindepth N] */

static const char *name_pat;
static int name_icase;
static char type_filter;
static int maxdepth = -1, mindepth = 0;

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

/* Shell-style wildcard match: * ? [set] */
static int glob_match(const char *p, const char *s) {
    for (; *p; p++, s++) {
        if (*p == '*') {
            while (p[1] == '*') p++;
            for (const char *t = s; ; t++) {
                if (glob_match(p + 1, t)) return 1;
                if (!*t) return 0;
            }
        }
        if (!*s) return 0;
        if (*p == '?') continue;
        if (*p == '[') {
            const char *q = p + 1;
            int neg = (*q == '!' || *q == '^');
            if (neg) q++;
            int hit = 0;
            for (; *q && *q != ']'; q++) {
                if (q[1] == '-' && q[2] && q[2] != ']') {
                    if (*s >= q[0] && *s <= q[2]) hit = 1;
                    q += 2;
                } else if (*q == *s) {
                    hit = 1;
                }
            }
            if (hit == neg) return 0;
            p = *q ? q : q - 1;
            continue;
        }
        if ((name_icase ? lower(*p) != lower(*s) : *p != *s)) return 0;
    }
    return *s == '\0';
}

static void visit(const char *path, int depth) {
    struct stat st;
    if (lstat(path, &st) < 0) {
        fprintf(stderr, "find: '%s': %s\n", path, strerror(errno));
        return;
    }
    const char *base = strrchr(path, '/');
    base = (base && base[1]) ? base + 1 : path;

    int ok = depth >= mindepth;
    if (ok && name_pat && !glob_match(name_pat, base)) ok = 0;
    if (ok && type_filter) {
        char t = S_ISDIR(st.st_mode) ? 'd' : S_ISCHR(st.st_mode) ? 'c' : 'f';
        if (t != type_filter) ok = 0;
    }
    if (ok) puts(path);

    if (!S_ISDIR(st.st_mode) || (maxdepth >= 0 && depth >= maxdepth)) return;
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char child[512];
        path_join(child, sizeof(child), path, de->d_name);
        visit(child, depth + 1);
    }
    closedir(d);
}

int main(int argc, char *argv[]) {
    int i = 1;
    const char *paths[32];
    int npaths = 0;
    for (; i < argc && argv[i][0] != '-' && npaths < 32; i++) paths[npaths++] = argv[i];
    for (; i < argc; i++) {
        const char *arg = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(argv[i], "-name") && arg) { name_pat = arg; i++; }
        else if (!strcmp(argv[i], "-iname") && arg) { name_pat = arg; name_icase = 1; i++; }
        else if (!strcmp(argv[i], "-type") && arg) { type_filter = arg[0]; i++; }
        else if (!strcmp(argv[i], "-maxdepth") && arg) { maxdepth = atoi(arg); i++; }
        else if (!strcmp(argv[i], "-mindepth") && arg) { mindepth = atoi(arg); i++; }
        else if (!strcmp(argv[i], "-print")) {}
        else {
            fprintf(stderr, "find: unknown predicate '%s'\n", argv[i]);
            return 1;
        }
    }
    if (npaths == 0) paths[npaths++] = ".";
    for (int k = 0; k < npaths; k++) visit(paths[k], 0);
    return 0;
}
