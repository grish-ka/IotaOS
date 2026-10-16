/*
* IotaOS - signal.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>

/* 1. Fundamental Signal & Process Types (Guarded against circular includes) */
#ifndef _PID_T_DEFINED
#define _PID_T_DEFINED
typedef int pid_t;
#endif

#if !defined(sigset_t) && !defined(_SIGSET_T_DEFINED)
#define _SIGSET_T_DEFINED
typedef int sigset_t;
#endif

#if !defined(sig_atomic_t) && !defined(_SIG_ATOMIC_T_DEFINED)
#define _SIG_ATOMIC_T_DEFINED
typedef int sig_atomic_t;
#endif

/* 2. Signal Handler Types & Special Constants */
typedef void (*sighandler_t)(int);
typedef sighandler_t __sighandler_t;

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

/* 3. Signal Numbers */
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGIOT    6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCLD    SIGCHLD
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   SIGIO
#define SIGPWR    30
#define SIGSYS    31
#define NSIG      32

/* 4. sigprocmask 'how' Constants */
#ifndef SIG_BLOCK
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
#endif

/* 5. sigaction Structure & Flags */
struct sigaction {
    sighandler_t sa_handler;
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};

#define SA_NOCLDSTOP 1
#define SA_NOCLDWAIT 2
#define SA_SIGINFO   4
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_NODEFER   0x40000000
#define SA_RESETHAND 0x80000000
#define SA_INTERRUPT 0x20000000

/* 6. Function Prototypes */
sighandler_t signal(int signum, sighandler_t handler);
int raise(int sig);
int kill(pid_t pid, int sig);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigsuspend(const sigset_t *mask);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigdelset(sigset_t *set, int signum);
int sigismember(const sigset_t *set, int signum);

#endif /* _SIGNAL_H */