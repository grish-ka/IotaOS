/*
* IotaOS - chmod.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <sys/stat.h>

/* Apply an octal mode or a symbolic one like u+x,go-w,a=r */
static int new_mode(const char *spec, mode_t old, mode_t *out) {
    if (*spec >= '0' && *spec <= '7') {
        char *end;
        long v = strtol(spec, &end, 8);
        if (*end) return -1;
        *out = v & 07777;
        return 0;
    }
    mode_t m = old;
    while (*spec) {
        mode_t who = 0;
        for (; *spec && strchr("ugoa", *spec); spec++) {
            if (*spec == 'u') who |= 04700;
            else if (*spec == 'g') who |= 02070;
            else if (*spec == 'o') who |= 01007;
            else who |= 07777;
        }
        if (!who) who = 07777 & ~022; /* like a umask of 022 */
        char op = *spec++;
        if (op != '+' && op != '-' && op != '=') return -1;
        mode_t bits = 0;
        for (; *spec && *spec != ','; spec++) {
            if (*spec == 'r') bits |= 0444;
            else if (*spec == 'w') bits |= 0222;
            else if (*spec == 'x') bits |= 0111;
            else if (*spec == 's') bits |= 06000;
            else if (*spec == 't') bits |= 01000;
            else return -1;
        }
        bits &= who;
        if (op == '+') m |= bits;
        else if (op == '-') m &= ~bits;
        else m = (m & ~(who & 0777)) | bits;
        if (*spec == ',') spec++;
    }
    *out = m;
    return 0;
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int i = 1, verbose = 0, status = 0;
    if (i < argc && !strcmp(argv[i], "-v")) { verbose = 1; i++; }
    if (argc - i < 2) {
        fprintf(stderr, "Usage: %s MODE FILE...\n", prog);
        return 1;
    }
    const char *spec = argv[i++];
    for (; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) < 0) {
            fprintf(stderr, "%s: cannot access '%s': %s\n", prog, argv[i], strerror(errno));
            status = 1;
            continue;
        }
        mode_t m;
        if (new_mode(spec, st.st_mode & 07777, &m) < 0) {
            fprintf(stderr, "%s: invalid mode: '%s'\n", prog, spec);
            return 1;
        }
        if (chmod(argv[i], m) < 0) {
            fprintf(stderr, "%s: changing permissions of '%s': %s\n", prog, argv[i], strerror(errno));
            status = 1;
        } else if (verbose) {
            printf("mode of '%s' changed to %04o\n", argv[i], m);
        }
    }
    return status;
}
