/*
 * IotaOS - syscall.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* System call numbers (int $0x80, number in EAX); mirrored in IotaADK's iota.h */
#define SYS_PANIC       0
#define SYS_REBOOT      2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_MEMINFO     5
#define SYS_EXIT        6
#define SYS_EXEC        7
#define SYS_LS          8
#define SYS_SLEEP       9
#define SYS_YIELD       10
#define SYS_SPAWN       11
#define SYS_PUTCHAR_AT  12
#define SYS_STRESS_RAM  13
#define SYS_CLEAR       14
#define SYS_BRK         15
#define SYS_OPEN        16
#define SYS_CLOSE       17
#define SYS_GET_ARGS    18
#define SYS_FORK        19
#define SYS_WAIT        20
#define SYS_GET_ENV     21
#define SYS_CHDIR       22
#define SYS_GETCWD      23
#define SYS_IOCTL       24
#define SYS_EXECVE      25
#define SYS_WAITPID     26
#define SYS_GETPID      27
#define SYS_GETPPID     28
#define SYS_STAT        29
#define SYS_LSEEK       30
#define SYS_DUP         31
#define SYS_DUP2        32
#define SYS_PIPE        33
#define SYS_FSTAT       34
#define SYS_MKDIR       35
#define SYS_UNLINK      36
#define SYS_RMDIR       37
#define SYS_RENAME      38
#define SYS_GETDENTS    39
#define SYS_FCNTL       40
#define SYS_TIME        41
#define SYS_PROCLIST    42
#define SYS_KILL        43
#define SYS_FTRUNCATE   44
#define SYS_SYSINFO     45
#define SYS_CHMOD       46
#define SYS_UTIME       47
#define SYS_POWEROFF    48
#define SYS_GETIDS      49
#define SYS_SETIDS      50
#define SYS_CHOWN       51

/* Sizes of the buffers handed to a starting program */
#define ARGS_SIZE 1024
#define ENV_SIZE  2048

/* Filled in by SYS_STAT / SYS_FSTAT */
typedef struct {
    uint32_t ino;
    uint32_t mode;   /* type bits (S_IFREG...) | permissions */
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t mtime;
    uint32_t rdev;
} kstat_t;

/* Filled in by SYS_SYSINFO */
typedef struct {
    uint32_t uptime_ticks;
    uint32_t ticks_per_second;
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t procs;
} ksysinfo_t;

extern char current_process_env[ENV_SIZE];

/* Start the program at path as a child of the current task; returns its pid or -1 */
int spawn_program(const char* path, const char* args);

#endif
