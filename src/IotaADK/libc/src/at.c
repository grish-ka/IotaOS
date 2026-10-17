/*
* IotaOS - at.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*
* The *at() family: a relative path is resolved against an open directory
* descriptor instead of the current directory. The kernel reports where the
* descriptor points (SYS_FDPATH) and the path is joined here.
*/

#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <utime.h>
#include "iota.h"

#define PATH_BUF 512

/* Turn (dirfd, path) into a path the plain calls understand */
static int resolve(int dirfd, const char *path, char *out) {
    if (!path) {
        errno = EFAULT;
        return -1;
    }
    if (path[0] == '/' || dirfd == AT_FDCWD) {
        if (strlen(path) >= PATH_BUF) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strcpy(out, path);
        return 0;
    }
    char dir[256];
    int r = iota_syscall3(IOTA_SYS_FDPATH, dirfd, (uint32_t)dir, 0);
    if (r < 0) {
        errno = -r;
        return -1;
    }
    size_t dl = strlen(dir);
    if (dl + strlen(path) + 2 > PATH_BUF) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(out, dir);
    if (dl > 0 && out[dl - 1] != '/') out[dl++] = '/';
    strcpy(out + dl, path);
    return 0;
}

int openat(int dirfd, const char *path, int oflag, ...) {
    char full[PATH_BUF];
    mode_t mode = 0;
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    if (resolve(dirfd, path, full) < 0) return -1;
    return open(full, oflag, mode);
}

int fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    return (flags & AT_SYMLINK_NOFOLLOW) ? lstat(full, buf) : stat(full, buf);
}

int mkdirat(int dirfd, const char *path, mode_t mode) {
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    return mkdir(full, mode);
}

int unlinkat(int dirfd, const char *path, int flags) {
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    return (flags & AT_REMOVEDIR) ? rmdir(full) : unlink(full);
}

int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
    char a[PATH_BUF], b[PATH_BUF];
    if (resolve(olddirfd, oldpath, a) < 0 || resolve(newdirfd, newpath, b) < 0) return -1;
    return rename(a, b);
}

int fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
    (void)flags;
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    return chmod(full, mode);
}

int fchownat(int dirfd, const char *path, uid_t owner, gid_t group, int flags) {
    (void)flags;
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    return chown(full, owner, group);
}

int faccessat(int dirfd, const char *path, int mode, int flags) {
    (void)flags;
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    return access(full, mode);
}

int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags) {
    (void)flags;
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    uint32_t mtime = 0;
    if (times && times[1].tv_nsec != UTIME_NOW) {
        if (times[1].tv_nsec == UTIME_OMIT) return 0;
        mtime = (uint32_t)times[1].tv_sec;
    }
    int r = iota_syscall3(IOTA_SYS_UTIME, (uint32_t)full, mtime, 0);
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return 0;
}

int mkfifoat(int dirfd, const char *path, mode_t mode) {
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    return mkfifo(full, mode);
}

int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev) {
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    return mknod(full, mode, dev);
}

/* No hard or symbolic links exist */
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags) {
    (void)olddirfd; (void)oldpath; (void)newdirfd; (void)newpath; (void)flags;
    errno = EPERM;
    return -1;
}

int symlinkat(const char *target, int newdirfd, const char *linkpath) {
    (void)target; (void)newdirfd; (void)linkpath;
    errno = EPERM;
    return -1;
}

ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz) {
    (void)buf; (void)bufsiz;
    char full[PATH_BUF];
    if (resolve(dirfd, path, full) < 0) return -1;
    struct stat st;
    if (stat(full, &st) < 0) return -1;
    errno = EINVAL; /* nothing is a symlink */
    return -1;
}
