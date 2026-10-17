/*
* IotaOS - mntent.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <mntent.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "iota.h"

FILE *setmntent(const char *filename, const char *type) {
    return fopen(filename, type);
}

int endmntent(FILE *stream) {
    if (stream) fclose(stream);
    return 1;
}

char *hasmntopt(const struct mntent *mnt, const char *opt) {
    return strstr(mnt->mnt_opts, opt);
}

int addmntent(FILE *stream, const struct mntent *mnt) {
    return fprintf(stream, "%s %s %s %s %d %d\n", mnt->mnt_fsname, mnt->mnt_dir, mnt->mnt_type,
                   mnt->mnt_opts, mnt->mnt_freq, mnt->mnt_passno) < 0;
}

/* Fields are whitespace separated: fsname dir type opts freq passno */
struct mntent *getmntent_r(FILE *stream, struct mntent *m, char *buf, int bufsize) {
    for (;;) {
        if (!fgets(buf, bufsize, stream)) return NULL;
        char *p = buf;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        char *f[6] = {0};
        int n = 0;
        while (n < 6 && *p && *p != '\n') {
            f[n++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            if (*p) *p++ = '\0';
            while (*p == ' ' || *p == '\t') p++;
        }
        if (n < 4) continue;
        m->mnt_fsname = f[0];
        m->mnt_dir = f[1];
        m->mnt_type = f[2];
        m->mnt_opts = f[3];
        m->mnt_freq = n > 4 ? atoi(f[4]) : 0;
        m->mnt_passno = n > 5 ? atoi(f[5]) : 0;
        return m;
    }
}

struct mntent *getmntent(FILE *stream) {
    static struct mntent m;
    static char buf[512];
    return getmntent_r(stream, &m, buf, sizeof(buf));
}

/* Everything lives in one RAM filesystem: sizes are the machine's memory */
int statvfs(const char *path, struct statvfs *buf) {
    iota_stat_t st;
    if (iota_stat(path, &st) < 0) {
        errno = ENOENT;
        return -1;
    }
    iota_sysinfo_t info;
    iota_syscall3(IOTA_SYS_SYSINFO, (uint32_t)&info, 0, 0);
    buf->f_bsize = buf->f_frsize = 4096;
    buf->f_blocks = info.total_pages;
    buf->f_bfree = buf->f_bavail = info.free_pages;
    buf->f_files = 1024;
    buf->f_ffree = buf->f_favail = 1024 - 64;
    buf->f_fsid = 1;
    buf->f_flag = 0;
    buf->f_namemax = 63;
    return 0;
}

int fstatvfs(int fd, struct statvfs *buf) {
    (void)fd;
    return statvfs("/", buf);
}

int statfs(const char *path, struct statfs *buf) {
    struct statvfs v;
    if (statvfs(path, &v) < 0) return -1;
    buf->f_type = TMPFS_MAGIC;
    buf->f_bsize = v.f_bsize;
    buf->f_blocks = v.f_blocks;
    buf->f_bfree = v.f_bfree;
    buf->f_bavail = v.f_bavail;
    buf->f_files = v.f_files;
    buf->f_ffree = v.f_ffree;
    buf->f_fsid.__val[0] = buf->f_fsid.__val[1] = 0;
    buf->f_namelen = 63;
    buf->f_frsize = v.f_frsize;
    buf->f_flags = 0;
    return 0;
}

int fstatfs(int fd, struct statfs *buf) {
    (void)fd;
    return statfs("/", buf);
}
