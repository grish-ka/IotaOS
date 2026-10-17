/*
* IotaOS - stat.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <sys/stat.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include "iota.h"

static mode_t current_umask = 022;

mode_t libc_current_umask(void) {
    return current_umask;
}

static void from_kernel(const iota_stat_t* k, struct stat* st) {
    memset(st, 0, sizeof(*st));
    st->st_dev = 1;
    st->st_ino = k->ino;
    st->st_mode = k->mode;
    st->st_nlink = k->nlink;
    st->st_uid = k->uid;
    st->st_gid = k->gid;
    st->st_rdev = k->rdev;
    st->st_size = k->size;
    st->st_atime = k->mtime;
    st->st_mtime = k->mtime;
    st->st_ctime = k->mtime;
    st->st_blksize = 4096;
    st->st_blocks = (k->size + 511) / 512;
}

static int check(int r) {
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return r;
}

int stat(const char *pathname, struct stat *statbuf) {
    iota_stat_t k;
    if (check(iota_syscall3(IOTA_SYS_STAT, (uint32_t)pathname, (uint32_t)&k, 0)) < 0) return -1;
    from_kernel(&k, statbuf);
    return 0;
}

/* There are no symbolic links */
int lstat(const char *pathname, struct stat *statbuf) {
    return stat(pathname, statbuf);
}

int fstat(int fd, struct stat *statbuf) {
    iota_stat_t k;
    if (check(iota_syscall3(IOTA_SYS_FSTAT, fd, (uint32_t)&k, 0)) < 0) return -1;
    from_kernel(&k, statbuf);
    return 0;
}

int chmod(const char *pathname, mode_t mode) {
    return check(iota_syscall3(IOTA_SYS_CHMOD, (uint32_t)pathname, mode, 0));
}

int fchmod(int fd, mode_t mode) {
    return check(iota_syscall3(IOTA_SYS_FCHMOD, fd, mode, 0));
}

int mknod(const char *pathname, mode_t mode, dev_t dev) {
    (void)pathname;
    (void)mode;
    (void)dev;
    errno = EPERM;
    return -1;
}

mode_t umask(mode_t mask) {
    mode_t old = current_umask;
    current_umask = mask & 0777;
    return old;
}

int mkdir(const char *pathname, mode_t mode) {
    return check(iota_syscall3(IOTA_SYS_MKDIR, (uint32_t)pathname, mode & ~current_umask & 07777, 0));
}

#include <utime.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

int utime(const char *path, const struct utimbuf *times) {
    return check(iota_syscall3(IOTA_SYS_UTIME, (uint32_t)path, times ? (uint32_t)times->modtime : 0, 0));
}

__attribute__((weak)) int utimes(const char *path, const struct timeval times[2]) {
    return check(iota_syscall3(IOTA_SYS_UTIME, (uint32_t)path, times ? (uint32_t)times[1].tv_sec : 0, 0));
}

__attribute__((weak)) int futimes(int fd, const struct timeval times[2]) {
    return check(iota_syscall3(IOTA_SYS_FUTIME, fd, times ? (uint32_t)times[1].tv_sec : 0, 0));
}

__attribute__((weak)) int futimens(int fd, const struct timespec times[2]) {
    uint32_t mtime = 0;
    if (times && times[1].tv_nsec != UTIME_NOW) {
        if (times[1].tv_nsec == UTIME_OMIT) return 0;
        mtime = (uint32_t)times[1].tv_sec;
    }
    return check(iota_syscall3(IOTA_SYS_FUTIME, fd, mtime, 0));
}

__attribute__((weak)) int mkfifo(const char *path, mode_t mode) {
    (void)path;
    (void)mode;
    errno = EPERM;
    return -1;
}
