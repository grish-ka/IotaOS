/*
* IotaOS - sort.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>

static int opt_reverse, opt_numeric, opt_unique, opt_fold;

static int compare(const void *a, const void *b) {
    const char *x = *(char *const *)a, *y = *(char *const *)b;
    int r;
    if (opt_numeric) {
        double dx = strtod(x, NULL), dy = strtod(y, NULL);
        r = dx < dy ? -1 : dx > dy ? 1 : strcmp(x, y);
    } else if (opt_fold) {
        r = strcasecmp(x, y);
    } else {
        r = strcmp(x, y);
    }
    return opt_reverse ? -r : r;
}

static char **lines;
static size_t nlines, cap;

static void read_lines(FILE *f) {
    char *line = NULL;
    size_t lcap = 0;
    ssize_t n;
    while ((n = getline(&line, &lcap, f)) > 0) {
        if (line[n - 1] == '\n') line[n - 1] = '\0';
        if (nlines == cap) {
            cap = cap ? cap * 2 : 256;
            lines = realloc(lines, sizeof(char *) * cap);
        }
        lines[nlines++] = strdup(line);
    }
    free(line);
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'r') opt_reverse = 1;
            else if (*p == 'n') opt_numeric = 1;
            else if (*p == 'u') opt_unique = 1;
            else if (*p == 'f') opt_fold = 1;
        }
    }
    if (i >= argc) {
        read_lines(stdin);
    } else {
        for (; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (!f) {
                warn_errno(prog, argv[i]);
                return 2;
            }
            read_lines(f);
            fclose(f);
        }
    }
    qsort(lines, nlines, sizeof(char *), compare);
    for (size_t k = 0; k < nlines; k++) {
        if (opt_unique && k > 0 && compare(&lines[k - 1], &lines[k]) == 0) continue;
        puts(lines[k]);
    }
    return 0;
}
