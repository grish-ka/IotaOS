/*
* IotaOS - cat.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>

static int number_lines;
static long line_no = 1;
static int at_line_start = 1;

static int cat_fd(int fd) {
    if (!number_lines) return copy_fd(fd, STDOUT_FILENO);
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (at_line_start) {
                printf("%6ld\t", line_no++);
                at_line_start = 0;
            }
            putchar(buf[i]);
            if (buf[i] == '\n') at_line_start = 1;
        }
    }
    return n < 0 ? -1 : 0;
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int status = 0, i = 1;
    if (i < argc && !strcmp(argv[i], "-n")) {
        number_lines = 1;
        i++;
    }
    if (i >= argc) return cat_fd(STDIN_FILENO) < 0;

    for (; i < argc; i++) {
        if (!strcmp(argv[i], "-")) {
            cat_fd(STDIN_FILENO);
            continue;
        }
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            warn_errno(prog, argv[i]);
            status = 1;
            continue;
        }
        if (cat_fd(fd) < 0) {
            warn_errno(prog, argv[i]);
            status = 1;
        }
        close(fd);
    }
    return status;
}
