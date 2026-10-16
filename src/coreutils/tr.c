/*
* IotaOS - tr.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

/* Expand ranges and escapes like a-z and \n into a flat character list */
static int expand(const char *spec, char *out) {
    int n = 0;
    for (const char *p = spec; *p && n < 255; p++) {
        int c = (unsigned char)*p;
        if (c == '\\' && p[1]) {
            p++;
            c = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p == 'r' ? '\r' : *p;
        }
        if (p[1] == '-' && p[2]) {
            int hi = (unsigned char)p[2];
            for (int x = c; x <= hi && n < 255; x++) out[n++] = (char)x;
            p += 2;
        } else {
            out[n++] = (char)c;
        }
    }
    return n;
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int delete = 0, squeeze = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'd') delete = 1;
            else if (*p == 's') squeeze = 1;
        }
    }
    if (i >= argc || (!delete && i + 1 >= argc && !squeeze)) {
        fprintf(stderr, "Usage: %s [-ds] SET1 [SET2]\n", prog);
        return 1;
    }

    char set1[256], set2[256];
    int n1 = expand(argv[i], set1);
    int n2 = (i + 1 < argc) ? expand(argv[i + 1], set2) : 0;
    int map[256];
    int in_set1[256] = {0};
    for (int c = 0; c < 256; c++) map[c] = c;
    for (int k = 0; k < n1; k++) {
        in_set1[(unsigned char)set1[k]] = 1;
        if (n2) map[(unsigned char)set1[k]] = (unsigned char)set2[k < n2 ? k : n2 - 1];
    }

    int c, last = -1;
    while ((c = getchar()) != EOF) {
        if (delete && in_set1[c]) continue;
        int out = delete ? c : map[c];
        if (squeeze && out == last && (n2 ? 1 : in_set1[c])) continue;
        putchar(out);
        last = out;
    }
    return 0;
}
