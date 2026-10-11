/*
 * IotaOS - elf_loader.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "elf_loader.h"
#include "stdio.h"
#include "../mem/pmm.h"
#include "../mem/paging.h" 
#include <stddef.h>

extern int current_exit_code;

static void elf_memcpy(uint8_t *dest, const uint8_t *src, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) dest[i] = src[i];
}

static void elf_memset(uint8_t *dest, uint8_t value, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) dest[i] = value;
}

int elf_load(void *file_data, uint32_t file_size, elf_image_t *image) {
    (void)file_size; /* Ignored for now */
    
    if (file_data == NULL || image == NULL) return -1;
    image->page_directory = NULL;
    image->entry = 0;

    const uint8_t *file = (const uint8_t *)file_data;
    const Elf32_Ehdr *header = (const Elf32_Ehdr *)file_data;
    
    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        printf("elf_loader: Invalid ELF Magic!\n");
        return -1;
    }

    uint32_t* new_dir = vmm_create_address_space();
    if (!new_dir) return -1;

    const Elf32_Phdr *programs = (const Elf32_Phdr *)(file + header->e_phoff);

    for (uint32_t i = 0; i < header->e_phnum; i++) {
        const Elf32_Phdr *program = &programs[i];
        if (program->p_type != PT_LOAD || program->p_memsz == 0) continue;

        uint32_t start_page = program->p_vaddr & ~0xFFF;
        uint32_t end_page = (program->p_vaddr + program->p_memsz + 0xFFF) & ~0xFFF;

        for (uint32_t virt = start_page; virt < end_page; virt += 4096) {
            uint32_t phys = (uint32_t)pmm_alloc_block();
            elf_memset((uint8_t*)phys, 0, 4096);
            vmm_map_page(new_dir, virt, phys, 7); 
        }
    }

    /* 1. Remember exactly whose memory space we are currently in! */
    uint32_t old_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));

    /* 2. Switch into the new app's universe to write the data */
    vmm_switch_directory(new_dir);

    for (uint32_t i = 0; i < header->e_phnum; i++) {
        const Elf32_Phdr *program = &programs[i];
        if (program->p_type != PT_LOAD || program->p_memsz == 0) continue;

        elf_memcpy((uint8_t*)program->p_vaddr, file + program->p_offset, program->p_filesz);
    }

    /* 3. Restore the EXACT caller's memory space (e.g. the Shell) */
    vmm_switch_directory((uint32_t*)old_cr3);

    image->page_directory = new_dir;
    image->entry = header->e_entry;
    return 0;
}

int elf_run(elf_image_t *image) {
    if (image == NULL || image->page_directory == NULL || image->entry == 0) return -1;
    current_exit_code = 0;
    
    /* Remember who called us (e.g. the Shell) so we can return to them safely */
    uint32_t old_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));
    
    vmm_switch_directory(image->page_directory);
    void (*app_entry)(void) = (void (*)(void))image->entry;
    app_entry();
    
    /* Go back to the exact directory that called us! */
    vmm_switch_directory((uint32_t*)old_cr3); 
    return current_exit_code;
}

void elf_unload(elf_image_t *image) {
    if (image == NULL || image->page_directory == NULL) return;
    
    /* We only need to clean up the private app table at index 32 (128 MB) */
    if (image->page_directory[32] & 1) { 
        uint32_t* table = (uint32_t*)(image->page_directory[32] & ~0xFFF);
        for (int j = 0; j < 1024; j++) {
            if (table[j] & 1) {
                pmm_free_block((void*)(table[j] & ~0xFFF));
            }
        }
        pmm_free_block(table);
    }
    
    pmm_free_block(image->page_directory);
    image->page_directory = NULL;
}

int elf_load_and_run(void *file_data, uint32_t file_size) {
    elf_image_t image;
    if (elf_load(file_data, file_size, &image) != 0) return -1;
    int exit_code = elf_run(&image);
    elf_unload(&image);
    return exit_code;
}