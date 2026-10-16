/*
* IotaOS - cp.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

static int recursive, verbose;
static const char *prog;

static int copy_file(const char *src, const char *dst, mode_t mode) {
    int in = open(src, O_RDONLY);
    if (in < 0) {
        fprintf(stderr, "%s: cannot open '%s' for reading: %s\n", prog, src, strerror(errno));
        return 1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode & 0777);
    if (out < 0) {
        fprintf(stderr, "%s: cannot create regular file '%s': %s\n", prog, dst, strerror(errno));
        close(in);
        return 1;
    }
    int r = copy_fd(in, out);
    close(in);
    close(out);
    chmod(dst, mode & 07777);
    if (r < 0) {
        fprintf(stderr, "%s: error copying '%s': %s\n", prog, src, strerror(errno));
        return 1;
    }
    if (verbose) printf("'%s' -> '%s'\n", src, dst);
    return 0;
}

static int copy_path(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) < 0) {
        fprintf(stderr, "%s: cannot stat '%s': %s\n", prog, src, strerror(errno));
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) return copy_file(src, dst, st.st_mode);

    if (!recursive) {
        fprintf(stderr, "%s: -r not specified; omitting directory '%s'\n", prog, src);
        return 1;
    }
    struct stat dst_st;
    if (stat(dst, &dst_st) < 0 && mkdir(dst, st.st_mode & 0777) < 0) {
        fprintf(stderr, "%s: cannot create directory '%s': %s\n", prog, dst, strerror(errno));
        return 1;
    }
    if (verbose) printf("'%s' -> '%s'\n", src, dst);

    DIR *d = opendir(src);
    if (!d) return 1;
    int status = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char s[512], t[512];
        path_join(s, sizeof(s), src, de->d_name);
        path_join(t, sizeof(t), dst, de->d_name);
        status |= copy_path(s, t);
    }
    closedir(d);
    return status;
}

int main(int argc, char *argv[]) {
    prog = prog_name(argv[0]);
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'r' || *p == 'R' || *p == 'a') recursive = 1;
            else if (*p == 'v') verbose = 1;
        }
    }
    int nargs = argc - i;
    if (nargs < 2) {
        fprintf(stderr, nargs ? "%s: missing destination file operand after '%s'\n"
                              : "%s: missing file operand\n", prog, nargs ? argv[i] : "");
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
        status |= copy_path(argv[i], target);
    }
    return status;
}
