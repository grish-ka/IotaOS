/*
* IotaOS - stat.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>
#include <time.h>

struct stat {
    int st_dev;
    int st_ino;
    unsigned int st_mode;
    int st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    int st_rdev;
    long st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
    long st_blksize;
    long st_blocks;
};

/* File type bitmasks */
#define S_IFMT   0170000
#define S_IFIFO  0010000
#define S_IFCHR  0020000
#define S_IFDIR  0040000
#define S_IFBLK  0060000
#define S_IFREG  0100000
#define S_IFLNK  0120000
#define S_IFSOCK 0140000

/* File type test macros */
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* Special mode bits & permissions */
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_IRWXU  0000700
#define S_IRUSR  0000400
#define S_IWUSR  0000200
#define S_IXUSR  0000100

#define S_IRWXG  0000070
#define S_IRGRP  0000040
#define S_IWGRP  0000020
#define S_IXGRP  0000010

#define S_IRWXO  0000007
#define S_IROTH  0000004
#define S_IWOTH  0000002
#define S_IXOTH  0000001

int stat(const char *pathname, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);
int chmod(const char *pathname, mode_t mode);

struct timespec;
#define UTIME_NOW  ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)
int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags);
int futimens(int fd, const struct timespec times[2]);
int fchmodat(int dirfd, const char *path, mode_t mode, int flags);
int fstatat(int dirfd, const char *path, struct stat *buf, int flags);
int mkdirat(int dirfd, const char *path, mode_t mode);
int mkfifo(const char *path, mode_t mode);
int mkfifoat(int dirfd, const char *path, mode_t mode);
int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev);
int fstat(int fd, struct stat *statbuf);
mode_t umask(mode_t mask);
int fchmod(int fd, mode_t mode);
int mkdir(const char *pathname, mode_t mode);
int mknod(const char *pathname, mode_t mode, dev_t dev);

#endif