/*
* IotaOS - resource.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <sys/resource.h>
#include <string.h>
#include <errno.h>

/* No resource limits are enforced; the open-file and process limits reflect the kernel's tables */
int getrlimit(int resource, struct rlimit *rlim) {
    rlim->rlim_cur = rlim->rlim_max = RLIM_INFINITY;
    if (resource == RLIMIT_NOFILE) rlim->rlim_cur = rlim->rlim_max = 32;
    else if (resource == RLIMIT_NPROC) rlim->rlim_cur = rlim->rlim_max = 31;
    else if (resource == RLIMIT_STACK) rlim->rlim_cur = rlim->rlim_max = 256 * 1024;
    return 0;
}

int setrlimit(int resource, const struct rlimit *rlim) {
    (void)resource;
    (void)rlim;
    return 0;
}

int getrusage(int who, struct rusage *usage) {
    (void)who;
    memset(usage, 0, sizeof(*usage));
    return 0;
}

int getpriority(int which, id_t who) {
    (void)which;
    (void)who;
    errno = 0;
    return 0;
}

int setpriority(int which, id_t who, int prio) {
    (void)which;
    (void)who;
    (void)prio;
    return 0;
}
