/*
* IotaOS - mkdir.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <sys/stat.h>

static int make_parents(char *path, mode_t mode) {
    for (char *p = path + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        struct stat st;
        if (stat(path, &st) < 0 && mkdir(path, mode) < 0) {
            *p = '/';
            return -1;
        }
        *p = '/';
    }
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    return mkdir(path, mode);
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int parents = 0, verbose = 0, status = 0, i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'p') parents = 1;
            else if (*p == 'v') verbose = 1;
        }
    }
    if (i >= argc) {
        fprintf(stderr, "%s: missing operand\n", prog);
        return 1;
    }
    for (; i < argc; i++) {
        int r = parents ? make_parents(argv[i], 0777) : mkdir(argv[i], 0777);
        if (r < 0) {
            fprintf(stderr, "%s: cannot create directory '%s': %s\n", prog, argv[i], strerror(errno));
            status = 1;
        } else if (verbose) {
            printf("%s: created directory '%s'\n", prog, argv[i]);
        }
    }
    return status;
}
