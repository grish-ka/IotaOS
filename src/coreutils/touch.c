/*
* IotaOS - touch.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "iota.h"

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int status = 0, no_create = 0, i = 1;
    if (i < argc && !strcmp(argv[i], "-c")) {
        no_create = 1;
        i++;
    }
    if (i >= argc) {
        fprintf(stderr, "%s: missing file operand\n", prog);
        return 1;
    }
    for (; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) == 0) {
            iota_syscall3(IOTA_SYS_UTIME, (uint32_t)argv[i], 0, 0);
            continue;
        }
        if (no_create) continue;
        int fd = open(argv[i], O_WRONLY | O_CREAT, 0666);
        if (fd < 0) {
            fprintf(stderr, "%s: cannot touch '%s': %s\n", prog, argv[i], strerror(errno));
            status = 1;
            continue;
        }
        close(fd);
    }
    return status;
}
