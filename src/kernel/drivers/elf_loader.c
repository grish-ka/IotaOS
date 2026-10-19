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

/* Map zeroed pages over [start, end) in dir; returns -1 if memory runs out */
static int map_zeroed(uint32_t *dir, uint32_t start, uint32_t end) {
    for (uint32_t virt = start & ~0xFFF; virt < end; virt += PAGE_SIZE) {
        if (vmm_get_phys(dir, virt)) continue; /* segments may share a page */
        uint32_t phys = (uint32_t)pmm_alloc_block();
        if (!phys) return -1;
        elf_memset((uint8_t*)phys, 0, PAGE_SIZE);
        vmm_map_page(dir, virt, phys, 7);
    }
    return 0;
}

/* Copy into another address space through the identity-mapped physical pages,
   so the loader never has to switch CR3 away from the stack it is running on */
static void copy_to_space(uint32_t *dir, uint32_t vaddr, const uint8_t *src, uint32_t len) {
    while (len > 0) {
        uint32_t chunk = PAGE_SIZE - (vaddr & 0xFFF);
        if (chunk > len) chunk = len;
        elf_memcpy((uint8_t*)vmm_get_phys(dir, vaddr), src, chunk);
        vaddr += chunk;
        src += chunk;
        len -= chunk;
    }
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
        return -1;
    }

    const Elf32_Phdr *programs = (const Elf32_Phdr *)(file + header->e_phoff);

    /* Everything must fit in the private region below the stack; mapping
       anywhere else would write into page tables shared with the kernel */
    for (uint32_t i = 0; i < header->e_phnum; i++) {
        const Elf32_Phdr *program = &programs[i];
        if (program->p_type != PT_LOAD || program->p_memsz == 0) continue;
        if (program->p_vaddr < APP_REGION_BASE ||
            program->p_vaddr + program->p_memsz > APP_ARGS_ADDR) {
            printf("elf_loader: Segment at 0x%x does not fit the app region!\n", program->p_vaddr);
            return -1;
        }
    }

    uint32_t* new_dir = vmm_create_address_space();
    if (!new_dir) { printf("elf_loader: no address space\n"); return -1; }

    for (uint32_t i = 0; i < header->e_phnum; i++) {
        const Elf32_Phdr *program = &programs[i];
        if (program->p_type != PT_LOAD || program->p_memsz == 0) continue;

        if (map_zeroed(new_dir, program->p_vaddr, program->p_vaddr + program->p_memsz) != 0) {
            printf("elf_loader: map seg 0x%x mem 0x%x failed\n", program->p_vaddr, program->p_memsz);
            vmm_free_address_space(new_dir);
            return -1;
        }
        copy_to_space(new_dir, program->p_vaddr, file + program->p_offset, program->p_filesz);
    }

    if (map_zeroed(new_dir, APP_ARGS_ADDR, APP_STACK_TOP) != 0) {
        printf("elf_loader: map args/stack failed\n");
        vmm_free_address_space(new_dir);
        return -1;
    }

    image->page_directory = new_dir;
    image->entry = header->e_entry;
    return 0;
}

/*
 * Replace the calling context with the image: switch to its address space
 * and its stack in one step, free the address space we came from, and jump
 * to the entry point. Programs leave through SYS_EXIT, so this never returns.
 */
int elf_run(elf_image_t *image) {
    if (image == NULL || image->page_directory == NULL || image->entry == 0) return -1;
    current_exit_code = 0;

    uint32_t old_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));

    __asm__ volatile (
        "mov %%eax, %%cr3\n"       /* no stack access until ESP is reloaded */
        "mov %%ecx, %%esp\n"
        "push %%edx\n"
        "call vmm_free_address_space\n"
        "add $4, %%esp\n"
        "sti\n"                    /* syscalls enter with IF=0; programs run with it set */
        "call *%%ebx\n"            /* EBX survived the call above (callee-saved) */
        "mov $6, %%eax\n"          /* entry returned: SYS_EXIT(0) */
        "xor %%ebx, %%ebx\n"
        "int $0x80\n"
        "1: hlt\n"
        "jmp 1b\n"
        :
        : "a"(image->page_directory), "c"(APP_STACK_TOP), "d"(old_cr3), "b"(image->entry)
        : "memory"
    );
    __builtin_unreachable();
}

void elf_unload(elf_image_t *image) {
    if (image == NULL || image->page_directory == NULL) return;
    vmm_free_address_space(image->page_directory);
    image->page_directory = NULL;
}

int elf_load_and_run(void *file_data, uint32_t file_size) {
    elf_image_t image;
    if (elf_load(file_data, file_size, &image) != 0) return -1;
    return elf_run(&image);
}
