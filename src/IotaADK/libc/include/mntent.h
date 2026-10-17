/*
* IotaOS - mntent.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _MNTENT_H
#define _MNTENT_H

#include <stdio.h>

#define MNTTAB     "/etc/fstab"
#define MOUNTED    "/etc/mtab"
#define MNTTYPE_IGNORE "ignore"
#define MNTOPT_DEFAULTS "defaults"
#define MNTOPT_RO "ro"
#define MNTOPT_RW "rw"

struct mntent {
    char *mnt_fsname;
    char *mnt_dir;
    char *mnt_type;
    char *mnt_opts;
    int mnt_freq;
    int mnt_passno;
};

FILE *setmntent(const char *filename, const char *type);
struct mntent *getmntent(FILE *stream);
struct mntent *getmntent_r(FILE *stream, struct mntent *result, char *buffer, int bufsize);
int addmntent(FILE *stream, const struct mntent *mnt);
int endmntent(FILE *stream);
char *hasmntopt(const struct mntent *mnt, const char *opt);

#endif
