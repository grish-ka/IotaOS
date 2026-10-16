/*
* IotaOS - cut.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

static char selected[256];

/* Parse a list like 1,3-5,7- into selected[] (1-based) */
static void parse_list(const char *list) {
    while (*list) {
        int lo = atoi(list), hi;
        if (*list == '-') lo = 1;
        while (*list >= '0' && *list <= '9') list++;
        if (*list == '-') {
            list++;
            hi = (*list >= '0' && *list <= '9') ? atoi(list) : 255;
            while (*list >= '0' && *list <= '9') list++;
        } else {
            hi = lo;
        }
        for (int k = lo; k <= hi && k < 256; k++) if (k > 0) selected[k] = 1;
        if (*list == ',') list++;
        else if (*list) break;
    }
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    char delim = '\t';
    int by_char = 0, have_list = 0;
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        char opt = argv[i][1];
        const char *val = argv[i][2] ? argv[i] + 2 : (i + 1 < argc ? argv[++i] : "");
        if (opt == 'd') delim = val[0];
        else if (opt == 'f' || opt == 'c' || opt == 'b') {
            by_char = (opt != 'f');
            parse_list(val);
            have_list = 1;
        }
    }
    if (!have_list) {
        fprintf(stderr, "%s: you must specify a list of bytes, characters, or fields\n", prog);
        return 1;
    }

    FILE *in = (i < argc) ? fopen(argv[i], "r") : stdin;
    if (!in) {
        warn_errno(prog, argv[i]);
        return 1;
    }
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    while ((n = getline(&line, &cap, in)) > 0) {
        if (line[n - 1] == '\n') line[--n] = '\0';
        if (by_char) {
            for (ssize_t k = 0; k < n; k++) if (k + 1 < 256 && selected[k + 1]) putchar(line[k]);
        } else if (!strchr(line, delim)) {
            fputs(line, stdout);
        } else {
            int field = 1, printed = 0;
            char *p = line;
            for (;;) {
                char *end = strchr(p, delim);
                if (field < 256 && selected[field]) {
                    if (printed++) putchar(delim);
                    fwrite(p, 1, end ? (size_t)(end - p) : strlen(p), stdout);
                }
                if (!end) break;
                p = end + 1;
                field++;
            }
        }
        putchar('\n');
    }
    free(line);
    return 0;
}
