/* 
 * IotaOS - pmm.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

/* * pmm_init: Sets up the bitmap.
 * mem_size: Total RAM in bytes.
 * bitmap_loc: Where in memory to store the bitmap itself.
 */
void pmm_init(uint32_t mem_size, uint32_t bitmap_loc);

/* Mark specific blocks as used or free */
void pmm_mark_used(uint32_t addr);
void pmm_mark_free(uint32_t addr);

/* Allocate/Free a single 4KB block */
void* pmm_alloc_block();
void pmm_free_block(void* addr);

uint32_t pmm_get_free_block_count(void);
uint32_t pmm_get_total_block_count(void);
uint32_t pmm_get_used_block_count(void);

#endif