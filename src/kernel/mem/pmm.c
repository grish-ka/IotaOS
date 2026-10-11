/* 
 * IotaOS - pmm.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "pmm.h"

uint32_t* pmm_bitmap;
uint32_t  max_blocks;
uint32_t  used_blocks;

void pmm_init(uint32_t mem_size, uint32_t bitmap_loc) {
    pmm_bitmap = (uint32_t*)bitmap_loc;
    max_blocks = mem_size / PAGE_SIZE;
    used_blocks = max_blocks; 

    /* 1. Start by marking EVERYTHING as used (1). 
       We will "un-mark" free RAM safely from kernel_main. */
    for (uint32_t i = 0; i < (max_blocks + 31) / 32; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }
}

void pmm_mark_used(uint32_t addr) {
    uint32_t block = addr / PAGE_SIZE;
    if (block >= max_blocks) return;
    
    /* Only increment the counter if the block was actually free */
    if (!(pmm_bitmap[block / 32] & (1U << (block % 32)))) {
        pmm_bitmap[block / 32] |= (1U << (block % 32));
        used_blocks++;
    }
}

void pmm_mark_free(uint32_t addr) {
    uint32_t block = addr / PAGE_SIZE;
    if (block >= max_blocks) return;
    
    /* Only decrement the counter if the block was actually used */
    if (pmm_bitmap[block / 32] & (1U << (block % 32))) {
        pmm_bitmap[block / 32] &= ~(1U << (block % 32));
        used_blocks--;
    }
}

void* pmm_alloc_block() {
    /* Search the bitmap for the first '0' bit (free page) */
    for (uint32_t i = 0; i < (max_blocks + 31) / 32; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) { // Optimization: only check if not full
            for (int j = 0; j < 32; j++) {
                uint32_t block = i * 32 + (uint32_t)j;
                if (block >= max_blocks) break;
                uint32_t bit = 1U << j;
                if (!(pmm_bitmap[i] & bit)) {
                    uint32_t addr = block * PAGE_SIZE;
                    pmm_mark_used(addr);
                    return (void*)addr;
                }
            }
        }
    }
    return NULL; // Out of memory!
}

void* pmm_alloc_blocks(uint32_t count) {
    if (count == 0 || count > max_blocks) return NULL;

    uint32_t last_start = max_blocks - count;
    for (uint32_t start = 0; start <= last_start; start++) {
        uint32_t page = 0;
        while (page < count) {
            uint32_t block = start + page;
            if (pmm_bitmap[block / 32] & (1U << (block % 32))) break;
            page++;
        }

        if (page == count) {
            uint32_t address = start * PAGE_SIZE;
            for (uint32_t i = 0; i < count; i++) {
                pmm_mark_used(address + i * PAGE_SIZE);
            }
            return (void*)address;
        }

        start += page;
    }

    return NULL;
}

void pmm_free_block(void* addr) {
    pmm_mark_free((uint32_t)addr);
}

/* --- Getters for your IOSH 'meminfo' command! --- */

uint32_t pmm_get_free_block_count(void) {
    return max_blocks - used_blocks;
}

uint32_t pmm_get_total_block_count(void) {
    return max_blocks;
}

uint32_t pmm_get_used_block_count(void) {
    return used_blocks;
}