/* * IotaOS - kernel.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "drivers/terminal.h"
#include "drivers/stdio.h"
#include "drivers/keyboard.h"
#include "drivers/string.h"
#include "drivers/system.h"
#include "drivers/ib_loader.h"
#include "cpu/idt.h"
#include "cpu/pic.h"
#include "mem/pmm.h"
#include "multiboot.h"
#include "fs/tar.h"

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This project needs to be compiled with a ix86-elf compiler"
#endif

/* Global variable to store where the ramdisk is in memory so the shell can use it */
uint32_t global_initrd_address = 0;

void kernel_main(uint32_t magic, uint32_t multiboot_info_addr)
{
    /* Initialize terminal interface */
    terminal_initialize();

    /* --- DEBUG MULTIBOOT PARSING --- */
    // printf("Multiboot Check: Magic=0x%x, Addr=0x%x\n", magic, multiboot_info_addr);

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        multiboot_info_t* mb_info = (multiboot_info_t*)multiboot_info_addr;
        
        // printf("Multiboot Flags: 0x%x, Modules: %d\n", mb_info->flags, mb_info->mods_count);

        /* Bit 3 is 0x8. Let's check if it's set. */
        if (mb_info->mods_count > 0) {
            /* inside kernel_main, after finding the module */
            multiboot_module_t* module = (multiboot_module_t*)mb_info->mods_addr;
            global_initrd_address = module->mod_start;

            // Protect the ramdisk so the PMM doesn't give this memory away!
            for (uint32_t addr = module->mod_start; addr < module->mod_end; addr += PAGE_SIZE) {
                pmm_mark_used(addr);
            }
            
            // printf("Success! Ramdisk found at 0x%x\n", global_initrd_address);
            tar_parse(global_initrd_address);
        } else {
            panic("Error: GRUB reports 0 modules loaded.\n");
        }
    } else {
        panic("Error: Magic number mismatch! Check boot.s pushes.\n");
    }
    pic_remap();
    idt_install();
    __asm__ volatile("sti");

    /* PMM Initialization (128MB RAM, Bitmap at 1MB) */
    pmm_init(128 * 1024 * 1024, 0x100000); 

    /* Mark the first 4MB as used (BIOS, VGA, Kernel, and the Bitmap) */
    for (uint32_t i = 0; i < 0x400000; i += PAGE_SIZE) {
        pmm_mark_used(i);
    }

    /* Mark the rest of the 128MB as FREE */
    for (uint32_t i = 0x400000; i < 128 * 1024 * 1024; i += PAGE_SIZE) {
        pmm_mark_free(i);
    }

    /* Double check marking the first 1MB as used */
    for(uint32_t i = 0; i < 0x100000; i += PAGE_SIZE) {
        pmm_mark_used(i);
    }

    char* IOTAOS_VERSION = "0.1.0-beta.";

    terminal_writestring("Hello, kernel World!\n");
    terminal_writestring("This is IotaOS, a simple 32-bit operating system kernel written in C.\n\n");

    printf(
        "  ___     _        ___  ____  \n"
        " |_ _|___ | |_ __ _ / _ \\/ ___| \n"
        "  | |/ _ \\| __/ _` | | | \\___ \\ \n"
        "  | | (_) | || (_| | |_| |___) |\n"
        " |___\\___/ \\__\\__,_|\\___/|____/ \n\n"
    );

    printf("Welcome to IotaOS Shell (IOSH)!\n");

    while (1) {
        printf("user@IotaOS$ ");
        char cmd[256];
        gets(cmd, sizeof(cmd));
        
        if (strcmp(cmd, "help") == 0) {
            printf("Available commands:\n");
            printf("  help    - Show this help message\n");
            printf("  ls      - List files in the ramdisk\n");
            printf("  run     - Execute an .ib file (e.g. 'run test.ib')\n");
            printf("  meminfo - Show physical memory usage information\n");
            printf("  version - Show the kernel and shell version\n");
            printf("  clear   - Clear the terminal screen\n");
            printf("  reboot  - Reboot the system\n");
            printf("  exit    - Exit the shell and shutdown the system\n");
        } 
        else if (strcmp(cmd, "ls") == 0) {
            if (global_initrd_address == 0) {
                printf("Error: No ramdisk loaded.\n");
            } else {
                tar_parse(global_initrd_address);
            }
        }
        else if (cmd[0] == '.' && cmd[1] == '/') {
            char* filename = &cmd[2]; /* Extract filename after "run " */
            
            if (global_initrd_address == 0) {
                printf("Error: No ramdisk loaded.\n");
            } else {
                void* file_data = tar_get_file(global_initrd_address, filename);
                if (file_data == NULL) {
                    printf("Error: File '%s' not found.\n", filename);
                } else {
                    ib_load_and_run(file_data);
                }
            }
        }
        else if (strcmp(cmd, "version") == 0) {
            printf("IotaOS Kernel Version: %s.\n", IOTAOS_VERSION);
            printf("Iota Shell (IOSH) Version: 0.1.0\n");
            printf("Copyright (c) 2026 grish-ka. Licensed under MIT.\n");
        }
        else if (strcmp(cmd, "clear") == 0) {
            terminal_clear();
        }
        else if (strcmp(cmd, "reboot") == 0) {
            reboot();
        }
        else if (strcmp(cmd, "exit") == 0) {
            shutdown();
        }
        else if (strcmp(cmd, "bsod") == 0) {
            __asm__ volatile("int $1"); /* Trigger a debug fault for testing the BSOD */
        }
        else if (strcmp(cmd, "meminfo") == 0) {
            uint32_t free = pmm_get_free_block_count();
            uint32_t total = pmm_get_total_block_count();
            printf("Physical Memory: %d/%d MB Free\n", (free * 4096) / 1024 / 1024, (total * 4096) / 1024 / 1024);
        }
        else if (cmd[0] != '\0') {
            printf("IOSH: Unknown command: %s\n", cmd);
        }
    }
}