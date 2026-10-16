/*
* IotaOS - dirent.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/types.h>

#define DT_UNKNOWN 0
#define DT_CHR     2
#define DT_DIR     4
#define DT_REG     8

struct dirent {
    unsigned long d_ino;
    unsigned char d_type;
    char d_name[256];
};

typedef struct {
    int fd;
    int pos;       /* next record in buf */
    int count;     /* records in buf */
    struct dirent entry;
    unsigned char buf[16 * 72]; /* raw kernel records */
} DIR;

DIR *opendir(const char *name);
DIR *fdopendir(int fd);
struct dirent *readdir(DIR *dirp);
void rewinddir(DIR *dirp);
int closedir(DIR *dirp);
int dirfd(DIR *dirp);

#endif
