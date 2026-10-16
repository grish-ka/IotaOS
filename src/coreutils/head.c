/*
* IotaOS - head.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>

static long count = 10;
static int bytes_mode;

static void head_fd(int fd) {
    char buf[4096];
    long left = count;
    ssize_t n;
    while (left > 0 && (n = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t take = 0;
        if (bytes_mode) {
            take = n < left ? n : left;
            left -= take;
        } else {
            while (take < n && left > 0) {
                if (buf[take++] == '\n') left--;
            }
        }
        write(STDOUT_FILENO, buf, take);
    }
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int i = 1, status = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if ((!strcmp(argv[i], "-n") || !strcmp(argv[i], "-c")) && i + 1 < argc) {
            bytes_mode = argv[i][1] == 'c';
            count = atol(argv[++i]);
        } else if (argv[i][1] == 'n' || argv[i][1] == 'c') {
            bytes_mode = argv[i][1] == 'c';
            count = atol(argv[i] + 2);
        } else {
            count = atol(argv[i] + 1);
        }
    }
    if (i >= argc) {
        head_fd(STDIN_FILENO);
        return 0;
    }
    int multiple = argc - i > 1;
    for (int first = 1; i < argc; i++, first = 0) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "%s: cannot open '%s' for reading: %s\n", prog, argv[i], strerror(errno));
            status = 1;
            continue;
        }
        if (multiple) printf("%s==> %s <==\n", first ? "" : "\n", argv[i]);
        head_fd(fd);
        close(fd);
    }
    return status;
}
