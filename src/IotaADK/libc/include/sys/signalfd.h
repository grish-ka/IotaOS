/*
* IotaOS - signalfd.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _SYS_SIGNALFD_H
#define _SYS_SIGNALFD_H

#include <stdint.h>
#include <signal.h>

#define SFD_CLOEXEC  0x80000
#define SFD_NONBLOCK 0x0800

struct signalfd_siginfo {
    uint32_t ssi_signo;
    int32_t  ssi_errno;
    int32_t  ssi_code;
    uint32_t ssi_pid;
    uint32_t ssi_uid;
    int32_t  ssi_fd;
    uint32_t ssi_tid;
    uint32_t ssi_band;
    uint32_t ssi_overrun;
    uint32_t ssi_trapno;
    int32_t  ssi_status;
    int32_t  ssi_int;
    uint64_t ssi_ptr;
    uint64_t ssi_utime;
    uint64_t ssi_stime;
    uint64_t ssi_addr;
    uint8_t  pad[48];
};

/* IotaOS does not deliver signals to programs yet, so the descriptor never
   becomes readable; it can still be polled alongside other descriptors. */
int signalfd(int fd, const sigset_t *mask, int flags);

#endif
