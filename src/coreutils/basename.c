/*
* IotaOS - basename.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "%s: missing operand\n", prog_name(argv[0]));
        return 1;
    }
    char path[256];
    snprintf(path, sizeof(path), "%s", argv[1]);
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') path[--len] = '\0';
    char *base = strrchr(path, '/');
    base = (base && base[1]) ? base + 1 : path;
    if (argc > 2) {
        size_t bl = strlen(base), sl = strlen(argv[2]);
        if (bl > sl && !strcmp(base + bl - sl, argv[2])) base[bl - sl] = '\0';
    }
    puts(base);
    return 0;
}
