/*
 * IotaOS - task.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "isr.h"

void task_init(void);
int create_task(void (*entry)(), uint32_t* page_directory);
uint32_t task_schedule(uint32_t current_esp);
int task_fork(registers_t *regs);

/* NEW: Process lifecycle management */
void task_exit(void);
void task_wait(int pid);

/* Expose current_task so syscalls can see the PID */
extern int current_task;

#endif