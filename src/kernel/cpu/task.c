/*
 * IotaOS - task.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "task.h"
#include "../mem/pmm.h"
#include "../mem/paging.h" 

typedef struct {
    uint32_t esp;
    uint32_t* page_directory; 
    volatile uint8_t active; /* FIX: Force the CPU to read from RAM every time */
} task_t;

#define MAX_TASKS 16
task_t tasks[MAX_TASKS];
int current_task = 0;
int task_count = 0;

static void task_returned(void) {
    tasks[current_task].active = 0;
    __asm__ volatile ("int $32");
    for (;;) __asm__ volatile ("hlt");
}

void task_init(void) {
    tasks[0].active = 1; 
    tasks[0].page_directory = kernel_directory; 
    task_count = 1;
}

int create_task(void (*entry)(), uint32_t* page_dir) {
    if (task_count >= MAX_TASKS) return -1;

    uint32_t *stack_page = pmm_alloc_block();
    if (stack_page == 0) return -1;
    uint32_t* stack = (uint32_t*)((uint32_t)stack_page + PAGE_SIZE);

    uint16_t current_cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(current_cs));

    *(--stack) = (uint32_t)task_returned;
    *(--stack) = 0x0202;         
    *(--stack) = current_cs;     
    *(--stack) = (uint32_t)entry;
    *(--stack) = 0;              
    *(--stack) = 32;             
    
    *(--stack) = 0; /* EAX */
    *(--stack) = 0; /* ECX */
    *(--stack) = 0; /* EDX */
    *(--stack) = 0; /* EBX */
    *(--stack) = 0; /* ESP */
    *(--stack) = 0; /* EBP */
    *(--stack) = 0; /* ESI */
    *(--stack) = 0; /* EDI */
    
    *(--stack) = 0; /* Dummy DS */

    tasks[task_count].esp = (uint32_t)stack;
    tasks[task_count].page_directory = page_dir; 
    tasks[task_count].active = 1;
    task_count++;
    return 0;
}

uint32_t task_schedule(uint32_t current_esp) {
    tasks[current_task].esp = current_esp;
    
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    tasks[current_task].page_directory = (uint32_t*)cr3;

    for (int checked = 0; checked < task_count; checked++) {
        current_task++;
        if (current_task >= task_count) current_task = 0;
        
        if (tasks[current_task].active) {
            vmm_switch_directory(tasks[current_task].page_directory);
            return tasks[current_task].esp;
        }
    }
    return current_esp;
}

int task_fork(registers_t *regs) {
    if (task_count >= MAX_TASKS) return -1;

    uint32_t parent_esp = (uint32_t)regs;
    uint32_t parent_stack_base = parent_esp & ~0xFFF;

    uint32_t new_stack = (uint32_t)pmm_alloc_block();
    if (!new_stack) return -1;

    uint8_t* src = (uint8_t*)parent_stack_base;
    uint8_t* dst = (uint8_t*)new_stack;
    for(int i = 0; i < 4096; i++) dst[i] = src[i];

    uint32_t child_esp = new_stack + (parent_esp & 0xFFF);

    registers_t* child_regs = (registers_t*)child_esp;
    child_regs->eax = 0;

    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint32_t* child_dir = vmm_clone_directory((uint32_t*)cr3);

    int child_pid = task_count;
    tasks[child_pid].esp = child_esp;
    tasks[child_pid].page_directory = child_dir;
    tasks[child_pid].active = 1;
    task_count++;

    return child_pid;
}

void task_exit(void) {
    tasks[current_task].active = 0;
    __asm__ volatile ("int $32");
}

void task_wait(int pid) {
    if (pid < 0 || pid >= MAX_TASKS) return;
    
    while (tasks[pid].active) {
        /* FIX: Add the memory clobber so GCC knows RAM might change during the interrupt */
        __asm__ volatile ("int $32" : : : "memory"); 
    }
}