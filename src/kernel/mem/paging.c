/*
 * IotaOS - paging.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "mem/paging.h"
#include "mem/pmm.h"
#include "drivers/stdio.h"

uint32_t* kernel_directory = 0;

static void vmm_memset(uint8_t* dest, uint8_t val, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) dest[i] = val;
}

void vmm_map_page(uint32_t* dir, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x03FF;

    if (!(dir[pd_idx] & PAGE_PRESENT)) {
        uint32_t* new_table = (uint32_t*)pmm_alloc_block();
        vmm_memset((uint8_t*)new_table, 0, PAGE_SIZE);
        dir[pd_idx] = (uint32_t)new_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    uint32_t* table = (uint32_t*)(dir[pd_idx] & ~0xFFF);
    table[pt_idx] = (paddr & ~0xFFF) | flags;
}

void paging_init(uint32_t total_memory) {
    kernel_directory = (uint32_t*)pmm_alloc_block();
    vmm_memset((uint8_t*)kernel_directory, 0, PAGE_SIZE);

    uint32_t num_pages = total_memory / PAGE_SIZE;
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t addr = i * PAGE_SIZE;
        vmm_map_page(kernel_directory, addr, addr, PAGE_PRESENT | PAGE_RW);
    }

    vmm_switch_directory(kernel_directory);
    printf("VMM: Paging initialized dynamically for %d MB!\n", total_memory / 1024 / 1024);
}

uint32_t* vmm_create_address_space(void) {
    uint32_t* new_dir = (uint32_t*)pmm_alloc_block();
    vmm_memset((uint8_t*)new_dir, 0, PAGE_SIZE);

    for (int i = 0; i < 1024; i++) {
        if (i == 32) {
            new_dir[i] = 0; 
        } else {
            new_dir[i] = kernel_directory[i];
        }
    }
    return new_dir;
}

void vmm_switch_directory(uint32_t* dir) {
    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r" (dir)
        : "eax"
    );
}

uint32_t* vmm_clone_directory(uint32_t* current_dir) {
    uint32_t* new_dir = (uint32_t*)pmm_alloc_block();
    vmm_memset((uint8_t*)new_dir, 0, PAGE_SIZE);

    for(int i = 0; i < 1024; i++) {
        if (current_dir[i] == kernel_directory[i]) {
            new_dir[i] = kernel_directory[i];
        } 
        else if (current_dir[i] & PAGE_PRESENT) {
            uint32_t* old_table = (uint32_t*)(current_dir[i] & ~0xFFF);
            uint32_t* new_table = (uint32_t*)pmm_alloc_block();
            vmm_memset((uint8_t*)new_table, 0, PAGE_SIZE);

            new_dir[i] = (uint32_t)new_table | (current_dir[i] & 0xFFF);

            for (int j = 0; j < 1024; j++) {
                if (old_table[j] & PAGE_PRESENT) {
                    uint32_t old_page = old_table[j] & ~0xFFF;
                    uint32_t new_page = (uint32_t)pmm_alloc_block();

                    uint8_t* src = (uint8_t*)old_page;
                    uint8_t* dst = (uint8_t*)new_page;
                    for(int k = 0; k < 4096; k++) dst[k] = src[k];

                    new_table[j] = new_page | (old_table[j] & 0xFFF);
                }
            }
        }
    }
    return new_dir;
}