/*
* IotaOS - signal.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "signal.h"
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "iota.h"

/*
 * The kernel has no signal delivery yet, so dispositions and the blocked
 * mask are tracked in userspace. raise() dispatches handlers synchronously.
 */
static struct sigaction sig_actions[NSIG];
static sigset_t sig_blocked;

#define SIG_VALID(s) ((s) > 0 && (s) < NSIG)
#define SIG_BIT(s)   (1u << ((s) - 1))

sighandler_t signal(int signum, sighandler_t handler) {
    struct sigaction act, old;
    act.sa_handler = handler;
    act.sa_mask = 0;
    act.sa_flags = 0;
    act.sa_restorer = NULL;
    if (sigaction(signum, &act, &old) < 0)
        return SIG_ERR;
    return old.sa_handler;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (!SIG_VALID(signum))
        return -1;
    if (oldact)
        *oldact = sig_actions[signum];
    if (act) {
        if (signum == SIGKILL || signum == SIGSTOP)
            return -1;
        sig_actions[signum] = *act;
    }
    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    if (oldset)
        *oldset = sig_blocked;
    if (!set)
        return 0;
    switch (how) {
    case SIG_BLOCK:   sig_blocked |= *set;  break;
    case SIG_UNBLOCK: sig_blocked &= ~*set; break;
    case SIG_SETMASK: sig_blocked = *set;   break;
    default: return -1;
    }
    return 0;
}

int sigsuspend(const sigset_t *mask) {
    (void)mask;
    /* No asynchronous signals can arrive; POSIX says this always returns -1 (EINTR). */
    return -1;
}

int sigemptyset(sigset_t *set) { *set = 0; return 0; }
int sigfillset(sigset_t *set)  { *set = ~0; return 0; }

int sigaddset(sigset_t *set, int signum) {
    if (!SIG_VALID(signum)) return -1;
    *set |= SIG_BIT(signum);
    return 0;
}

int sigdelset(sigset_t *set, int signum) {
    if (!SIG_VALID(signum)) return -1;
    *set &= ~SIG_BIT(signum);
    return 0;
}

int sigismember(const sigset_t *set, int signum) {
    if (!SIG_VALID(signum)) return -1;
    return (*set & SIG_BIT(signum)) != 0;
}

int raise(int sig) {
    sighandler_t h;
    if (!SIG_VALID(sig))
        return -1;
    h = sig_actions[sig].sa_handler;
    if (h == SIG_IGN)
        return 0;
    if (h != SIG_DFL && h != NULL) {
        h(sig);
        return 0;
    }
    /* Default action: these just terminate, the rest are ignored */
    if (sig == SIGHUP || sig == SIGINT || sig == SIGQUIT || sig == SIGKILL || sig == SIGTERM ||
        sig == SIGABRT || sig == SIGPIPE || sig == SIGALRM || sig == SIGUSR1 || sig == SIGUSR2) {
        return kill(getpid(), sig);
    }
    return 0;
}

/* The kernel only implements termination: any signal but 0 ends the target */
int kill(pid_t pid, int sig) {
    if (pid == getpid() || pid == 0) {
        sighandler_t h = (SIG_VALID(sig)) ? sig_actions[sig].sa_handler : SIG_DFL;
        if (h == SIG_IGN) return 0;
        if (h != SIG_DFL && h != NULL) {
            h(sig);
            return 0;
        }
        pid = getpid();
    }
    int r = iota_syscall3(IOTA_SYS_KILL, pid, sig, 0);
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return 0;
}

int sigblock(int mask) {
    int old = sig_blocked;
    sig_blocked |= mask;
    return old;
}

int sigsetmask(int mask) {
    int old = sig_blocked;
    sig_blocked = mask;
    return old;
}

int signalfd(int fd, const sigset_t *mask, int flags) {
    (void)mask;
    if (fd != -1) return fd; /* changing the mask of an existing one */
    int p[2];
    if (pipe(p) < 0) return -1;
    /* The write end is kept open and never written: no signals ever arrive */
    fcntl(p[1], F_SETFD, FD_CLOEXEC);
    if (flags & 0x80000) fcntl(p[0], F_SETFD, FD_CLOEXEC);
    if (flags & 0x0800) fcntl(p[0], F_SETFL, O_NONBLOCK);
    return p[0];
}

/* No signal is ever delivered, so waiting for one could never finish:
   report EINVAL (POSIX returns the error number directly) */
int sigwait(const sigset_t *set, int *sig) {
    (void)set;
    (void)sig;
    return EINVAL;
}

int sigpending(sigset_t *set) {
    *set = 0;
    return 0;
}

__attribute__((weak)) int killpg(pid_t pgrp, int sig) {
    return kill(pgrp, sig);
}
