/*
 * IotaOS - paging.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "mem/paging.h"
#include "mem/pmm.h"
#include "drivers/stdio.h"
#include "drivers/fb.h"

uint32_t* kernel_directory = 0;

/*
 * Does dir share the kernel's page table at index i? Compare the table
 * address only: the CPU sets the Accessed bit in each directory's entry
 * independently, so whole entries drift apart even for the same table.
 */
static int is_kernel_table(uint32_t* dir, int i) {
    return (kernel_directory[i] & PAGE_PRESENT) &&
           (dir[i] & ~0xFFF) == (kernel_directory[i] & ~0xFFF);
}

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

    /* Map linear framebuffer if active (including high PCI addresses) */
    if (fb_info.enabled) {
        uint32_t fb_start = fb_info.paddr & ~0xFFF;
        uint32_t fb_end = (fb_info.paddr + fb_info.size + 0xFFF) & ~0xFFF;
        for (uint32_t a = fb_start; a < fb_end; a += PAGE_SIZE) {
            vmm_map_page(kernel_directory, a, a, PAGE_PRESENT | PAGE_RW | PAGE_USER);
        }
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
        if (is_kernel_table(current_dir, i)) {
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

uint32_t vmm_get_phys(uint32_t* dir, uint32_t vaddr) {
    uint32_t pde = dir[vaddr >> 22];
    if (!(pde & PAGE_PRESENT)) return 0;
    uint32_t* table = (uint32_t*)(pde & ~0xFFF);
    uint32_t pte = table[(vaddr >> 12) & 0x03FF];
    if (!(pte & PAGE_PRESENT)) return 0;
    return (pte & ~0xFFF) | (vaddr & 0xFFF);
}

void vmm_free_address_space(uint32_t* dir) {
    if (!dir || dir == kernel_directory) return;

    for (int i = 0; i < 1024; i++) {
        if (!(dir[i] & PAGE_PRESENT) || is_kernel_table(dir, i)) continue;
        uint32_t* table = (uint32_t*)(dir[i] & ~0xFFF);
        for (int j = 0; j < 1024; j++) {
            if (table[j] & PAGE_PRESENT) pmm_free_block((void*)(table[j] & ~0xFFF));
        }
        pmm_free_block(table);
    }
    pmm_free_block(dir);
}
