/*
* IotaOS - tail.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>

static long count = 10;

/* Read the whole input, then print its last count lines */
static void tail_fd(int fd) {
    size_t cap = 4096, len = 0;
    char *data = malloc(cap);
    ssize_t n;
    for (;;) {
        if (len == cap) {
            cap *= 2;
            data = realloc(data, cap);
        }
        n = read(fd, data + len, cap - len);
        if (n <= 0) break;
        len += n;
    }

    size_t start = len;
    long lines = 0;
    if (start > 0 && data[start - 1] == '\n') start--;
    while (start > 0) {
        if (data[start - 1] == '\n' && ++lines == count) break;
        start--;
    }
    write(STDOUT_FILENO, data + start, len - start);
    free(data);
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int i = 1, status = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) count = atol(argv[++i]);
        else if (argv[i][1] == 'n') count = atol(argv[i] + 2);
        else count = atol(argv[i] + 1);
    }
    if (count <= 0) return 0;
    if (i >= argc) {
        tail_fd(STDIN_FILENO);
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
        tail_fd(fd);
        close(fd);
    }
    return status;
}
