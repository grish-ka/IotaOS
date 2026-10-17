/*
* IotaOS - utime.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _UTIME_H
#define _UTIME_H

#include <sys/types.h>

struct utimbuf {
    time_t actime;
    time_t modtime;
};

/* Only the modification time is stored */
int utime(const char *path, const struct utimbuf *times);

#endif
