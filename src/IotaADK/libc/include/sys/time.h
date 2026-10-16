/*
* IotaOS - time.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <sys/types.h>

#ifndef _STRUCT_TIMEVAL_DEFINED
#define _STRUCT_TIMEVAL_DEFINED
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
#endif

#ifndef _STRUCT_TIMEZONE_DEFINED
#define _STRUCT_TIMEZONE_DEFINED
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

#ifndef _STRUCT_ITIMERVAL_DEFINED
#define _STRUCT_ITIMERVAL_DEFINED
struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};
#endif

#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2

#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000
#endif

int gettimeofday(struct timeval *tv, struct timezone *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);
int getitimer(int which, struct itimerval *curr_value);
int setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value);
int utimes(const char *filename, const struct timeval times[2]);

#endif /* _SYS_TIME_H */