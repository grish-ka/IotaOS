/*
* IotaOS - dirent.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <dirent.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "iota.h"

DIR *fdopendir(int fd) {
    DIR *d = malloc(sizeof(DIR));
    if (!d) {
        errno = ENOMEM;
        return NULL;
    }
    d->fd = fd;
    d->pos = 0;
    d->count = 0;
    return d;
}

DIR *opendir(const char *name) {
    int fd = open(name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return NULL;
    DIR *d = fdopendir(fd);
    if (!d) close(fd);
    return d;
}

struct dirent *readdir(DIR *dirp) {
    if (dirp->pos >= dirp->count) {
        int r = iota_syscall3(IOTA_SYS_GETDENTS, dirp->fd, (uint32_t)dirp->buf, sizeof(dirp->buf));
        if (r < 0) {
            errno = -r;
            return NULL;
        }
        dirp->count = r / sizeof(iota_dirent_t);
        dirp->pos = 0;
        if (dirp->count == 0) return NULL;
    }

    iota_dirent_t *k = (iota_dirent_t *)dirp->buf + dirp->pos++;
    dirp->entry.d_ino = k->ino;
    dirp->entry.d_type = k->type == 2 ? DT_DIR : k->type == 3 ? DT_CHR : DT_REG;
    strncpy(dirp->entry.d_name, k->name, sizeof(dirp->entry.d_name) - 1);
    dirp->entry.d_name[sizeof(dirp->entry.d_name) - 1] = '\0';
    return &dirp->entry;
}

void rewinddir(DIR *dirp) {
    lseek(dirp->fd, 0, SEEK_SET);
    dirp->pos = dirp->count = 0;
}

int closedir(DIR *dirp) {
    if (!dirp) {
        errno = EBADF;
        return -1;
    }
    int r = close(dirp->fd);
    free(dirp);
    return r;
}

int dirfd(DIR *dirp) {
    return dirp->fd;
}
