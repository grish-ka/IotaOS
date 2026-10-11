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

/* Switch the CPU to a new page directory */
void vmm_switch_directory(uint32_t* dir);

/* Global pointer to the kernel's master directory */
extern uint32_t* kernel_directory;

#endif