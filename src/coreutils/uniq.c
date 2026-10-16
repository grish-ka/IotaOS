/*
* IotaOS - uniq.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

int main(int argc, char *argv[]) {
    int count = 0, only_dups = 0, only_unique = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'c') count = 1;
            else if (*p == 'd') only_dups = 1;
            else if (*p == 'u') only_unique = 1;
        }
    }
    FILE *in = stdin;
    if (i < argc && strcmp(argv[i], "-")) {
        in = fopen(argv[i], "r");
        if (!in) {
            warn_errno(prog_name(argv[0]), argv[i]);
            return 1;
        }
    }

    char *prev = NULL, *line = NULL;
    size_t cap = 0;
    long reps = 0;
    ssize_t n;
    for (;;) {
        n = getline(&line, &cap, in);
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';
        if (prev && (n <= 0 || strcmp(prev, line))) {
            if ((!only_dups || reps > 1) && (!only_unique || reps == 1)) {
                if (count) printf("%7ld %s\n", reps, prev);
                else puts(prev);
            }
            free(prev);
            prev = NULL;
        }
        if (n <= 0) break;
        if (!prev) {
            prev = strdup(line);
            reps = 0;
        }
        reps++;
    }
    free(line);
    return 0;
}
