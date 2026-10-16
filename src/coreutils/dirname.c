/*
* IotaOS - dirname.c
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
    char *slash = strrchr(path, '/');
    if (!slash) puts(".");
    else if (slash == path) puts("/");
    else {
        *slash = '\0';
        puts(path);
    }
    return 0;
}
