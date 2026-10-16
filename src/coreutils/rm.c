/*
* IotaOS - rm.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <dirent.h>
#include <sys/stat.h>

static int recursive, force, verbose;
static const char *prog;

static int remove_path(const char *path) {
    struct stat st;
    if (lstat(path, &st) < 0) {
        if (force && errno == ENOENT) return 0;
        fprintf(stderr, "%s: cannot remove '%s': %s\n", prog, path, strerror(errno));
        return 1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (!recursive) {
            fprintf(stderr, "%s: cannot remove '%s': Is a directory\n", prog, path);
            return 1;
        }
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        if (!strcmp(base, ".") || !strcmp(base, "..")) {
            fprintf(stderr, "%s: refusing to remove '.' or '..' directory: skipping '%s'\n", prog, path);
            return 1;
        }

        int status = 0;
        /* Collect the names first: removing while reading would shift the listing */
        DIR *d = opendir(path);
        if (d) {
            char **names = NULL;
            int n = 0, cap = 0;
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
                if (n == cap) {
                    cap = cap ? cap * 2 : 16;
                    names = realloc(names, sizeof(char *) * cap);
                }
                names[n++] = strdup(de->d_name);
            }
            closedir(d);
            for (int i = 0; i < n; i++) {
                char child[512];
                path_join(child, sizeof(child), path, names[i]);
                status |= remove_path(child);
                free(names[i]);
            }
            free(names);
        }
        if (rmdir(path) < 0) {
            fprintf(stderr, "%s: cannot remove '%s': %s\n", prog, path, strerror(errno));
            return 1;
        }
        if (verbose) printf("removed directory '%s'\n", path);
        return status;
    }

    if (unlink(path) < 0) {
        fprintf(stderr, "%s: cannot remove '%s': %s\n", prog, path, strerror(errno));
        return 1;
    }
    if (verbose) printf("removed '%s'\n", path);
    return 0;
}

int main(int argc, char *argv[]) {
    prog = prog_name(argv[0]);
    int i = 1, status = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'r' || *p == 'R') recursive = 1;
            else if (*p == 'f') force = 1;
            else if (*p == 'v') verbose = 1;
            else {
                fprintf(stderr, "%s: invalid option -- '%c'\n", prog, *p);
                return 1;
            }
        }
    }
    if (i >= argc) {
        if (force) return 0;
        fprintf(stderr, "%s: missing operand\n", prog);
        return 1;
    }
    for (; i < argc; i++) status |= remove_path(argv[i]);
    return status;
}
