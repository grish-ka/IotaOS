/*
 * IotaOS - task.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "task.h"
#include "../mem/pmm.h"

typedef struct {
    uint32_t esp;
    uint8_t active;
} task_t;

#define MAX_TASKS 16
task_t tasks[MAX_TASKS];
int current_task = 0;
int task_count = 0;

void task_init(void) {
    /* Task 0 is whatever code is currently running when init is called (the kernel/shell) */
    tasks[0].active = 1; 
    task_count = 1;
}

void create_task(void (*entry)()) {
    if (task_count >= MAX_TASKS) return;

    /* Allocate a brand new 4KB block of physical RAM for the stack */
    uint32_t* stack = (uint32_t*)((uint32_t)pmm_alloc_block() + PAGE_SIZE);

    /* Grab the exact Code Segment the CPU is currently using to prevent GPFs! */
    uint16_t current_cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(current_cs));

    /* We manually format the very top of this empty stack so it perfectly 
       matches what the CPU expects to see when returning from an interrupt! */
       
    *(--stack) = 0x0202;         /* EFLAGS (Interrupts Enabled!) */
    *(--stack) = current_cs;     /* CS (Dynamically grabbed!) */
    *(--stack) = (uint32_t)entry;/* EIP (The function this task will run) */
    *(--stack) = 0;              /* Error Code (Dummy) */
    *(--stack) = 32;             /* IRQ Number (Dummy 32 for timer) */
    
    /* pushal registers */
    *(--stack) = 0; /* EAX */
    *(--stack) = 0; /* ECX */
    *(--stack) = 0; /* EDX */
    *(--stack) = 0; /* EBX */
    *(--stack) = 0; /* ESP (Ignored by popal) */
    *(--stack) = 0; /* EBP */
    *(--stack) = 0; /* ESI */
    *(--stack) = 0; /* EDI */
    
    *(--stack) = 0; /* Dummy DS */

    /* Save this fake stack into our array! */
    tasks[task_count].esp = (uint32_t)stack;
    tasks[task_count].active = 1;
    task_count++;
}

uint32_t task_schedule(uint32_t current_esp) {
    if (task_count <= 1) return current_esp; /* Nothing else to run */

    /* 1. Freeze! Save the current task's state */
    tasks[current_task].esp = current_esp;

    /* 2. Round-Robin Scheduling: Pick the next available task */
    current_task++;
    if (current_task >= task_count) {
        current_task = 0;
    }

    /* 3. Resume! Return the new task's stack pointer */
    return tasks[current_task].esp;
}