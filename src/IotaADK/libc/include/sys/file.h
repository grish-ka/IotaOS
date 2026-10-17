/*
* IotaOS - file.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _SYS_FILE_H
#define _SYS_FILE_H

#include <fcntl.h>

#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8

/* Advisory locks: only one user is ever writing, so locking always succeeds */
int flock(int fd, int operation);

#endif
