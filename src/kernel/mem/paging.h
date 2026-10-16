/*
 * IotaOS - paging.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/* Page Table Entry Flags */
#define PAGE_PRESENT  0x01   /* Is this page actually in RAM? */
#define PAGE_RW       0x02   /* 0 = Read Only, 1 = Read/Write */
#define PAGE_USER     0x04   /* 0 = Kernel Only, 1 = Userland App Access */

/* Initialize the kernel's virtual memory manager */
void paging_init(uint32_t total_memory);

/* Map a virtual address to a physical address in a specific directory */
void vmm_map_page(uint32_t* dir, uint32_t vaddr, uint32_t paddr, uint32_t flags);

/* Create a brand new address space (cloning the kernel's base mappings) */
uint32_t* vmm_create_address_space(void);

/* Clone an existing user address space entirely */
uint32_t* vmm_clone_directory(uint32_t* current_dir);

/* Physical address backing vaddr in dir, or 0 if it is not mapped */
uint32_t vmm_get_phys(uint32_t* dir, uint32_t vaddr);

/* Free a process directory with its private tables and pages (never the kernel's) */
void vmm_free_address_space(uint32_t* dir);

/*
 * Per-process private region: page directory entry 32 (128 MB - 132 MB).
 * Programs are linked at its base and their stack sits at its top, so a
 * cloned directory has the stack at the same virtual address (fork-safe).
 */
#define APP_REGION_BASE 0x08000000
#define APP_STACK_TOP   0x08400000
#define APP_STACK_PAGES 64  /* 256 KB */
#define APP_STACK_BASE  (APP_STACK_TOP - APP_STACK_PAGES * 4096)
/* One page under the stack holds the program's argv and environment blocks */
#define APP_ARGS_ADDR   (APP_STACK_BASE - 4096)
#define APP_ARGS_OFFSET 0     /* NUL-separated argv, up to 1024 bytes */
#define APP_ENV_OFFSET  1024  /* NUL-separated environment, up to 2048 bytes */

/* Switch the CPU to a new page directory */
void vmm_switch_directory(uint32_t* dir);

/* Global pointer to the kernel's master directory */
extern uint32_t* kernel_directory;

#endif