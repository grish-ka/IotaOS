/*
* IotaOS - mv.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int verbose = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'v') verbose = 1;
        }
    }
    int nargs = argc - i;
    if (nargs < 2) {
        fprintf(stderr, "%s: missing file operand\n", prog);
        return 1;
    }

    const char *dest = argv[argc - 1];
    struct stat st;
    int dest_is_dir = stat(dest, &st) == 0 && S_ISDIR(st.st_mode);
    if (nargs > 2 && !dest_is_dir) {
        fprintf(stderr, "%s: target '%s' is not a directory\n", prog, dest);
        return 1;
    }

    int status = 0;
    for (; i < argc - 1; i++) {
        char target[512];
        if (dest_is_dir) {
            const char *base = strrchr(argv[i], '/');
            path_join(target, sizeof(target), dest, base && base[1] ? base + 1 : argv[i]);
        } else {
            snprintf(target, sizeof(target), "%s", dest);
        }
        if (rename(argv[i], target) < 0) {
            fprintf(stderr, "%s: cannot move '%s' to '%s': %s\n", prog, argv[i], target, strerror(errno));
            status = 1;
        } else if (verbose) {
            printf("renamed '%s' -> '%s'\n", argv[i], target);
        }
    }
    return status;
}
