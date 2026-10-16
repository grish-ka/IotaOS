/*
 * IotaOS - task.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "task.h"
#include "../mem/pmm.h"
#include "../mem/paging.h"
#include "../drivers/string.h"
#include "../drivers/timer.h"

typedef struct {
    uint32_t esp;
    uint32_t* page_directory;
    volatile uint8_t active;   /* runnable; task_wait() spins until this clears */
    uint8_t in_use;            /* slot holds a task, running or awaiting reaping */
    volatile uint8_t waiting;  /* blocked in waitpid for a child (it is in the background) */
    uint8_t session_leader;    /* login shell started by init: Ctrl-C never kills it */
    char cwd[VFS_PATH_MAX];
    char name[TASK_NAME_MAX];
    uint32_t heap_end; /* program break, private to the task's address space */
    int ppid;          /* parent task, or -1 */
    int status;        /* wait status, valid once active == 0 until the parent collects it */
    uint32_t start_tick;
    uint32_t cpu_ticks;  /* timer ticks spent running */
    uint32_t uid;
    uint32_t gid;
    fd_table_t fds;
} task_t;

task_t tasks[MAX_TASKS];
int current_task = 0;

/*
 * Page directory the interrupt stub must load after switching ESP, or 0.
 * CR3 cannot change in C: task stacks live in per-process memory, so the
 * old stack disappears the moment the new directory is loaded.
 */
uint32_t scheduler_next_cr3 = 0;

static void task_returned(void) {
    task_exit(0);
    for (;;) __asm__ volatile ("hlt");
}

void task_init(void) {
    tasks[0].active = 1;
    tasks[0].in_use = 1;
    tasks[0].page_directory = kernel_directory;
    strcpy(tasks[0].cwd, "/");
    strcpy(tasks[0].name, "init");
    tasks[0].heap_end = TASK_HEAP_BASE;
    tasks[0].ppid = -1;
}

static int alloc_slot(void) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (!tasks[i].in_use) return i;
    }
    return -1;
}

static void init_child(int pid, int parent) {
    tasks[pid].in_use = 1;
    tasks[pid].active = 1;
    tasks[pid].waiting = 0;
    tasks[pid].session_leader = 0;
    strcpy(tasks[pid].cwd, tasks[parent].cwd);
    strcpy(tasks[pid].name, tasks[parent].name);
    tasks[pid].ppid = parent;
    tasks[pid].status = 0;
    tasks[pid].uid = tasks[parent].uid;
    tasks[pid].gid = tasks[parent].gid;
    tasks[pid].start_tick = timer_get_ticks();
    tasks[pid].cpu_ticks = 0;
    fd_clone(&tasks[pid].fds, &tasks[parent].fds);
}

/* Start an ELF image loaded by elf_load(); its stack is already mapped at APP_STACK_TOP */
int create_task(void (*entry)(), uint32_t* page_dir) {
    int pid = alloc_slot();
    if (pid < 0) return -1;

    /* Build the initial interrupt frame in the image's top stack page, through
       its physical address, for the scheduler stub to pop */
    uint32_t top_page = vmm_get_phys(page_dir, APP_STACK_TOP - PAGE_SIZE);
    if (!top_page) return -1;
    uint32_t* stack = (uint32_t*)(top_page + PAGE_SIZE);

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

    uint32_t frame_size = (top_page + PAGE_SIZE) - (uint32_t)stack;
    tasks[pid].esp = APP_STACK_TOP - frame_size;
    tasks[pid].page_directory = page_dir;
    tasks[pid].heap_end = TASK_HEAP_BASE;
    init_child(pid, current_task);
    return pid;
}

static int parent_alive(int pid) {
    int ppid = tasks[pid].ppid;
    return ppid >= 0 && tasks[ppid].in_use && tasks[ppid].active;
}

/*
 * Free the address spaces of tasks that have exited (never the one running).
 * The slot itself stays as a zombie holding the exit status until the parent
 * collects it with waitpid, unless there is no parent left to do so.
 */
static void reap_tasks(void) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (i == current_task || !tasks[i].in_use || tasks[i].active) continue;
        if (tasks[i].page_directory) {
            vmm_free_address_space(tasks[i].page_directory);
            tasks[i].page_directory = 0;
        }
        if (!parent_alive(i)) tasks[i].in_use = 0;
    }
}

uint32_t task_schedule(uint32_t current_esp) {
    tasks[current_task].esp = current_esp;

    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    tasks[current_task].page_directory = (uint32_t*)cr3;

    reap_tasks();

    for (int checked = 0; checked < MAX_TASKS; checked++) {
        current_task++;
        if (current_task >= MAX_TASKS) current_task = 0;

        if (tasks[current_task].in_use && tasks[current_task].active) {
            if ((uint32_t)tasks[current_task].page_directory != cr3) {
                scheduler_next_cr3 = (uint32_t)tasks[current_task].page_directory;
            }
            return tasks[current_task].esp;
        }
    }
    return current_esp;
}

int task_fork(registers_t *regs) {
    uint32_t parent_esp = (uint32_t)regs;

    /* Only programs fork, and their stack is in the private region, which the
       clone duplicates at the same virtual address */
    if (parent_esp < APP_STACK_BASE || parent_esp >= APP_STACK_TOP) return -1;

    int child_pid = alloc_slot();
    if (child_pid < 0) return -1;

    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint32_t* child_dir = vmm_clone_directory((uint32_t*)cr3);
    if (!child_dir) return -1;

    /* The child resumes from the same syscall frame, with fork() returning 0 */
    uint32_t eax_addr = (uint32_t)&regs->eax;
    *(uint32_t*)vmm_get_phys(child_dir, eax_addr) = 0;

    tasks[child_pid].esp = parent_esp;
    tasks[child_pid].page_directory = child_dir;
    /* The cloned directory holds a copy of the parent's heap pages */
    tasks[child_pid].heap_end = tasks[current_task].heap_end;
    init_child(child_pid, current_task);

    return child_pid;
}

/* Mark pid finished with the given wait status and drop its open files */
static void finish_task(int pid, int status) {
    fd_close_all(&tasks[pid].fds);
    tasks[pid].status = status;
    tasks[pid].waiting = 0;
    tasks[pid].active = 0;
}

void task_exit(int status) {
    finish_task(current_task, status);
    task_yield();
}

int task_kill(int pid, int sig) {
    if (pid <= 0 || pid >= MAX_TASKS || !tasks[pid].in_use || !tasks[pid].active) return -3; /* ESRCH */
    if (sig == 0) return 0;
    finish_task(pid, sig & 0x7f);
    if (pid == current_task) task_yield();
    return 0;
}

int task_interrupt_foreground(void) {
    /* The foreground jobs are the running tasks whose parent is blocked
       waiting for them; shells waiting on their own children are spared */
    int killed = 0;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (!tasks[i].in_use || !tasks[i].active || tasks[i].waiting) continue;
        if (tasks[i].session_leader) continue;
        int ppid = tasks[i].ppid;
        if (ppid < 0 || !tasks[ppid].in_use || !tasks[ppid].waiting) continue;
        finish_task(i, 2); /* SIGINT */
        killed++;
    }
    return killed;
}

/* Release a zombie whose exit status has been collected */
static void release_task(int pid) {
    if (tasks[pid].page_directory && pid != current_task) {
        vmm_free_address_space(tasks[pid].page_directory);
        tasks[pid].page_directory = 0;
    }
    tasks[pid].in_use = 0;
}

void task_wait(int pid) {
    if (pid <= 0 || pid >= MAX_TASKS) return;

    tasks[current_task].waiting = 1;
    while (tasks[pid].in_use && tasks[pid].active) {
        task_yield();
    }
    tasks[current_task].waiting = 0;
    if (tasks[pid].in_use && tasks[pid].ppid == current_task) release_task(pid);
}

int task_waitpid(int pid, int* status, int nohang) {
    for (;;) {
        int have_child = 0;
        for (int i = 1; i < MAX_TASKS; i++) {
            if (!tasks[i].in_use || tasks[i].ppid != current_task) continue;
            if (pid != -1 && pid != i) continue;
            have_child = 1;
            if (!tasks[i].active) {
                *status = tasks[i].status;
                release_task(i);
                tasks[current_task].waiting = 0;
                return i;
            }
        }
        if (!have_child || nohang) {
            tasks[current_task].waiting = 0;
            return have_child ? 0 : -1;
        }
        tasks[current_task].waiting = 1;
        task_yield();
    }
}

/* Set while the running task is halted waiting for input or a timer */
volatile int task_cpu_idle = 0;

void task_account_tick(void) {
    if (!task_cpu_idle && !tasks[current_task].waiting) tasks[current_task].cpu_ticks++;
}

int task_current_active(void) {
    return tasks[current_task].active;
}

uint32_t task_get_uid(void) { return tasks[current_task].uid; }
uint32_t task_get_gid(void) { return tasks[current_task].gid; }

int task_set_ids(uint32_t uid, uint32_t gid) {
    /* Only root may become someone else */
    if (tasks[current_task].uid != 0 && (uid != tasks[current_task].uid || gid != tasks[current_task].gid)) return -1;
    tasks[current_task].uid = uid;
    tasks[current_task].gid = gid;
    return 0;
}

void task_force_ids(uint32_t uid, uint32_t gid) {
    tasks[current_task].uid = uid;
    tasks[current_task].gid = gid;
}

int task_get_ppid(void) {
    return tasks[current_task].ppid;
}

void task_set_session_leader(int pid) {
    if (pid > 0 && pid < MAX_TASKS) tasks[pid].session_leader = 1;
}

fd_table_t* task_get_fds(void) {
    return &tasks[current_task].fds;
}

void task_set_name(const char* name) {
    int i = 0;
    for (; name[i] && i < TASK_NAME_MAX - 1; i++) tasks[current_task].name[i] = name[i];
    tasks[current_task].name[i] = '\0';
}

int task_list(task_info_t* out, int max) {
    int n = 0;
    for (int i = 0; i < MAX_TASKS && n < max; i++) {
        if (!tasks[i].in_use) continue;
        out[n].pid = i;
        out[n].ppid = tasks[i].ppid;
        out[n].state = tasks[i].active ? (tasks[i].waiting ? 'S' : (i == current_task ? 'R' : 'S')) : 'Z';
        out[n].start_tick = tasks[i].start_tick;
        out[n].cpu_ticks = tasks[i].cpu_ticks;
        out[n].uid = tasks[i].uid;
        strcpy(out[n].name, tasks[i].name);
        n++;
    }
    return n;
}

char* task_get_cwd(void) {
    return tasks[current_task].cwd;
}

void task_set_cwd(const char* path) {
    strcpy(tasks[current_task].cwd, path);
}

uint32_t task_get_heap_end(void) {
    return tasks[current_task].heap_end;
}

void task_set_heap_end(uint32_t heap_end) {
    tasks[current_task].heap_end = heap_end;
}
