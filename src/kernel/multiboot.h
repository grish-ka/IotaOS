/* * IotaOS - multiboot.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* This is the structure GRUB passes to us in the EBX register */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    /* We are ignoring the rest of the massive struct for now to keep it simple! */
} __attribute__((packed)) multiboot_info_t;

/* This structure describes each module (like your initrd.img) */
typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t pad;
} __attribute__((packed)) multiboot_module_t;

#endif