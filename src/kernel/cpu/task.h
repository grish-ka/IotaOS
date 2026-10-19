/*
 * IotaOS - task.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "isr.h"
#include "../fs/file.h"

#define MAX_TASKS 32
#define TASK_NAME_MAX 32

/* Where a fresh address space's heap (sbrk) begins (2 GB boundary) */
#define TASK_HEAP_BASE 0x80000000

/* One row of the process list (SYS_PROCLIST), as `ps` reads it */
typedef struct {
    int pid;
    int ppid;
    char state;         /* R running, S sleeping, Z zombie */
    uint32_t start_tick;
    char name[TASK_NAME_MAX];
    uint32_t cpu_ticks;
    uint32_t uid;
} task_info_t;

/* Give up the CPU to the next runnable task */
static inline void task_yield(void) {
    __asm__ volatile ("int $0x81" : : : "memory");
}

void task_init(void);
/* Start a loaded image as a child of the current task; returns its pid or -1 */
int create_task(void (*entry)(), uint32_t* page_directory);
uint32_t task_schedule(uint32_t current_esp);
int task_fork(registers_t *regs);

/* status is a wait status: (code << 8) for exit(code), or the signal number */
void task_exit(int status);
int task_kill(int pid, int sig);
/* Ctrl-C: terminate the foreground jobs; returns how many were killed */
int task_interrupt_foreground(void);
void task_wait(int pid);
/* Collect an exited child (pid -1 = any): its pid, 0 if none yet and nohang, -1 if no such child */
int task_waitpid(int pid, int* status, int nohang);
int task_current_active(void);
/* Charge the current timer tick to the running task (unless it is idle) */
void task_account_tick(void);
extern volatile int task_cpu_idle;
int task_get_ppid(void);
uint32_t task_get_uid(void);
uint32_t task_get_gid(void);
/* Change the current task's user and group; returns -1 if not permitted */
int task_set_ids(uint32_t uid, uint32_t gid);
/* Change ids without a permission check (set-user-ID exec) */
void task_force_ids(uint32_t uid, uint32_t gid);
void task_set_session_leader(int pid);

fd_table_t* task_get_fds(void);
void task_set_name(const char* name);
int task_list(task_info_t* out, int max);

char* task_get_cwd(void);
void task_set_cwd(const char* path);
uint32_t task_get_heap_end(void);
void task_set_heap_end(uint32_t heap_end);

extern int current_task;

#endif
