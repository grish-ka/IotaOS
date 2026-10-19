/* 
 * IotaOS - multiboot.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define MULTIBOOT_INFO_MEMORY             0x00000001
#define MULTIBOOT_INFO_BOOTDEV            0x00000002
#define MULTIBOOT_INFO_CMDLINE            0x00000004
#define MULTIBOOT_INFO_MODS               0x00000008
#define MULTIBOOT_INFO_AOUT_SYMS          0x00000010
#define MULTIBOOT_INFO_ELF_SHDR           0x00000020
#define MULTIBOOT_INFO_MEM_MAP            0x00000040
#define MULTIBOOT_INFO_DRIVE_INFO         0x00000080
#define MULTIBOOT_INFO_CONFIG_TABLE       0x00000100
#define MULTIBOOT_INFO_BOOT_LOADER_NAME   0x00000200
#define MULTIBOOT_INFO_APM_TABLE          0x00000400
#define MULTIBOOT_INFO_VBE_INFO           0x00000800
#define MULTIBOOT_INFO_FRAMEBUFFER_INFO   0x00001000

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED  0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB      1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

/* Multiboot Memory Map Entry Types (E820) */
#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

/* Multiboot Memory Map Entry structure */
typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

/* VBE 2.0+ Mode Information Block structure */
typedef struct {
    uint16_t attributes;
    uint8_t  winA, winB;
    uint16_t granularity;
    uint16_t winsize;
    uint16_t segmentA, segmentB;
    uint32_t realFctPtr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t  w_char, y_char, planes, bpp, banks;
    uint8_t  memory_model, bank_size, image_pages;
    uint8_t  reserved0;
    uint8_t  red_mask, red_position;
    uint8_t  green_mask, green_position;
    uint8_t  blue_mask, blue_position;
    uint8_t  rsv_mask, rsv_position;
    uint8_t  directcolor_attributes;
    uint32_t physbase;
    uint32_t reserved1;
    uint16_t reserved2;
} __attribute__((packed)) vbe_mode_info_t;

/* This is the structure GRUB passes to us in the EBX register */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;

    /* Symbols / ELF sections */
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;

    /* Memory map */
    uint32_t mmap_length;
    uint32_t mmap_addr;

    /* Drive Info */
    uint32_t drives_length;
    uint32_t drives_addr;

    /* ROM configuration table */
    uint32_t config_table;

    /* Boot Loader Name */
    uint32_t boot_loader_name;

    /* APM table */
    uint32_t apm_table;

    /* Video BIOS Extensions (VBE) */
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    /* Framebuffer (Multiboot 1 - flags bit 12) */
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  color_info[6];
} __attribute__((packed)) multiboot_info_t;

/* This structure describes each module (like your initrd.img) */
typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t pad;
} __attribute__((packed)) multiboot_module_t;

#endif