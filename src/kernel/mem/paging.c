/*
 * IotaOS - paging.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "mem/pmm.h"
#include "drivers/stdio.h"
#include <stdint.h>

/* Page Directory must be 4KB aligned */
uint32_t page_directory[1024] __attribute__((aligned(4096)));

/* 64 page tables to cover 256MB of RAM (64 * 4MB = 256MB) */
uint32_t page_tables[64][1024] __attribute__((aligned(4096)));

void paging_init() {
    /* 1. Set each entry to not present (2 = Read/Write, but not present) */
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002;
    }

    /* 2. Identity map the first 256MB of RAM! */
    uint32_t physical_addr = 0;
    for (int table_idx = 0; table_idx < 64; table_idx++) {
        for (int i = 0; i < 1024; i++) {
            /* 3 = Present + Read/Write */
            page_tables[table_idx][i] = physical_addr | 3;
            physical_addr += 4096;
        }
        /* Add the table to the directory */
        page_directory[table_idx] = ((uint32_t)page_tables[table_idx]) | 3;
    }

    /* 3. Load the directory and turn Paging on using inline assembly! */
    __asm__ volatile (
        "mov %0, %%cr3\n"     /* Load page directory into CR3 */
        "mov %%cr0, %%eax\n"  /* Read CR0 */
        "or $0x80000000, %%eax\n" /* Set the PG bit (Bit 31) */
        "mov %%eax, %%cr0\n"  /* Write back to CR0 to enable paging */
        :
        : "r" (page_directory)
        : "eax"
    );

    printf("Paging enabled successfully! (256MB Mapped)\n");
}