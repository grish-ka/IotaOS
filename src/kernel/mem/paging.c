/*
 * IotaOS - paging.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "mem/paging.h"
#include "mem/pmm.h"
#include "drivers/stdio.h"

uint32_t* kernel_directory = 0;

/* Helper to safely zero out memory for new page tables */
static void vmm_memset(uint8_t* dest, uint8_t val, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) dest[i] = val;
}

void vmm_map_page(uint32_t* dir, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    /* 1. Calculate the indices into the Directory and the Table */
    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x03FF;

    /* 2. If the Page Table doesn't exist yet, ask the PMM for a new physical block! */
    if (!(dir[pd_idx] & PAGE_PRESENT)) {
        uint32_t* new_table = (uint32_t*)pmm_alloc_block();
        vmm_memset((uint8_t*)new_table, 0, PAGE_SIZE);
        
        /* Map the new table into the directory. 
           We use PAGE_USER here so user apps can access their own tables! */
        dir[pd_idx] = (uint32_t)new_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    /* 3. Get the physical address of the Page Table (stripping the flags from the bottom 12 bits) */
    uint32_t* table = (uint32_t*)(dir[pd_idx] & ~0xFFF);
    
    /* 4. Set the physical address and flags into the Table! */
    table[pt_idx] = (paddr & ~0xFFF) | flags;
}

void paging_init(uint32_t total_memory) {
    /* Create the master directory for the Kernel */
    kernel_directory = (uint32_t*)pmm_alloc_block();
    vmm_memset((uint8_t*)kernel_directory, 0, PAGE_SIZE);

    /* Identity Map (Virtual = Physical) all of physical RAM for the kernel's use.
       This ensures the kernel can read/write to anywhere in RAM without faulting. */
    uint32_t num_pages = total_memory / PAGE_SIZE;
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t addr = i * PAGE_SIZE;
        /* Note: We DO NOT use PAGE_USER here. Apps cannot access kernel memory! */
        vmm_map_page(kernel_directory, addr, addr, PAGE_PRESENT | PAGE_RW);
    }

    /* Load the directory into the CPU and turn Virtual Memory on! */
    vmm_switch_directory(kernel_directory);

    printf("VMM: Paging initialized dynamically for %d MB!\n", total_memory / 1024 / 1024);
}

uint32_t* vmm_create_address_space(void) {
    uint32_t* new_dir = (uint32_t*)pmm_alloc_block();
    vmm_memset((uint8_t*)new_dir, 0, PAGE_SIZE);

    for (int i = 0; i < 1024; i++) {
        /* Poke a hole at 128 MB (Index 32) for the app! */
        if (i == 32) {
            new_dir[i] = 0; 
        } else {
            new_dir[i] = kernel_directory[i];
        }
    }
    return new_dir;
}

void vmm_switch_directory(uint32_t* dir) {
    /* Load the physical address of the directory into CR3 */
    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n" /* Set the Paging Enable (PG) bit */
        "mov %%eax, %%cr0\n"
        :
        : "r" (dir)
        : "eax"
    );
}