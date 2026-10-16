/*
* IotaOS - wait.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG   1
#define WUNTRACED 2

/* Status layout: exit code in bits 8-15; low 7 bits hold a terminating signal */
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WSTOPSIG(s)    WEXITSTATUS(s)
#define WIFEXITED(s)   (WTERMSIG(s) == 0)
#define WIFSTOPPED(s)  (((s) & 0xff) == 0x7f)
#define WIFSIGNALED(s) (!WIFEXITED(s) && !WIFSTOPPED(s))

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);

#endif
