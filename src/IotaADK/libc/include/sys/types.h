/*
* IotaOS - types.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* 1. Fundamental Process, User & Group Types */
#ifndef _PID_T_DEFINED
#define _PID_T_DEFINED
typedef int pid_t;
#endif

#ifndef _UID_T_DEFINED
#define _UID_T_DEFINED
typedef unsigned int uid_t;
typedef unsigned int gid_t;
#endif

/* 2. File System & Device Types */
#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef long off_t;
#endif

#ifndef _MODE_T_DEFINED
#define _MODE_T_DEFINED
typedef unsigned int mode_t;
#endif

#ifndef _DEV_T_DEFINED
#define _DEV_T_DEFINED
typedef unsigned int dev_t;
#endif

#ifndef _INO_T_DEFINED
#define _INO_T_DEFINED
typedef unsigned int ino_t;
typedef unsigned int nlink_t;
typedef long blksize_t;
typedef long blkcnt_t;
#endif

/* 3. Signed Size & Pointer Types */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef int ssize_t;
#endif

#ifndef _UINTPTR_T_DEFINED
#define _UINTPTR_T_DEFINED
typedef unsigned int uintptr_t;
typedef int intptr_t;
#endif

/* 4. Time & Clock Types */
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
typedef long time_t;
#endif

#ifndef _SUSECONDS_T_DEFINED
#define _SUSECONDS_T_DEFINED
typedef long suseconds_t;
typedef unsigned int useconds_t;
#endif

#if !defined(clock_t) && !defined(_CLOCK_T_DEFINED)
#define _CLOCK_T_DEFINED
typedef long clock_t;
#endif

#ifndef _STRUCT_TIMEVAL_DEFINED
#define _STRUCT_TIMEVAL_DEFINED
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
#endif

#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000
#endif

/* 5. Signal Set Types */
#if !defined(sigset_t) && !defined(_SIGSET_T_DEFINED)
#define _SIGSET_T_DEFINED
typedef int sigset_t;
#endif

#if !defined(sig_atomic_t) && !defined(_SIG_ATOMIC_T_DEFINED)
#define _SIG_ATOMIC_T_DEFINED
typedef int sig_atomic_t;
#endif

/* 6. Legacy BSD / System V Shorthand Types (used by Bash & Readline) */
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef char *caddr_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long long u_int64_t;

/* 7. File Descriptor Sets (for select / pselect) */
#ifndef _FD_SET_DEFINED
#define _FD_SET_DEFINED
#define FD_SETSIZE 64

typedef struct {
    unsigned int fds_bits[FD_SETSIZE / 32];
} fd_set;

#define FD_ZERO(set) \
    do { \
        int __i; \
        for (__i = 0; __i < (FD_SETSIZE / 32); __i++) \
            (set)->fds_bits[__i] = 0; \
    } while (0)

#define FD_SET(fd, set)   ((set)->fds_bits[(fd) / 32] |= (1U << ((fd) % 32)))
#define FD_CLR(fd, set)   ((set)->fds_bits[(fd) / 32] &= ~(1U << ((fd) % 32)))
#define FD_ISSET(fd, set) (((set)->fds_bits[(fd) / 32] & (1U << ((fd) % 32))) != 0)

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
#endif

/* 8. Expose <signal.h> AFTER all fundamental typedefs are complete */
#include <signal.h>

#endif /* _SYS_TYPES_H */