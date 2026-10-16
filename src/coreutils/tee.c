/*
* IotaOS - tee.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int append = 0, i = 1, status = 0;
    if (i < argc && !strcmp(argv[i], "-a")) {
        append = 1;
        i++;
    }
    int fds[32], nfds = 0;
    for (; i < argc && nfds < 32; i++) {
        int fd = open(argv[i], O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0666);
        if (fd < 0) {
            warn_errno(prog, argv[i]);
            status = 1;
            continue;
        }
        fds[nfds++] = fd;
    }
    char buf[4096];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, n);
        for (int k = 0; k < nfds; k++) write(fds[k], buf, n);
    }
    for (int k = 0; k < nfds; k++) close(fds[k]);
    return status;
}
