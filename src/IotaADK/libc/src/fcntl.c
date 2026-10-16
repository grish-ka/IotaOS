/*
* IotaOS - fcntl.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "fcntl.h"
#include "iota.h"
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>

/* The process umask lives in stat.c */
extern mode_t libc_current_umask(void);

int open(const char *path, int oflag, ...) {
    mode_t mode = 0;
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    mode &= ~libc_current_umask();

    int fd = iota_syscall3(IOTA_SYS_OPEN, (uint32_t)path, oflag, mode);
    if (fd < 0) {
        errno = -fd;
        return -1;
    }
    return fd;
}

int creat(const char *path, mode_t mode) {
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int fcntl(int fd, int cmd, ...) {
    int arg = 0;
    va_list ap;
    va_start(ap, cmd);
    arg = va_arg(ap, int);
    va_end(ap);

    int r = iota_syscall3(IOTA_SYS_FCNTL, fd, cmd, arg);
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return r;
}
