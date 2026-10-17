/*
* IotaOS - libgen.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <libgen.h>
#include <string.h>

/* Weak: some packages carry their own */
__attribute__((weak)) char *basename(char *path) {
    if (!path || !*path) return ".";
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') path[--len] = '\0';
    if (len == 1 && path[0] == '/') return path;
    char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

__attribute__((weak)) char *dirname(char *path) {
    if (!path || !*path) return ".";
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') path[--len] = '\0';
    char *slash = strrchr(path, '/');
    if (!slash) return ".";
    while (slash > path && slash[-1] == '/') slash--;
    if (slash == path) {
        path[1] = '\0';
        return path;
    }
    *slash = '\0';
    return path;
}
