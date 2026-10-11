/* 
 * IotaOS - kernel.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdint.h>
#include "drivers/terminal.h"
#include "drivers/stdio.h"
#include "drivers/keyboard.h"
#include "drivers/string.h"
#include "drivers/system.h"
#include "drivers/elf_loader.h"
#include "drivers/timer.h"    
#include "cpu/idt.h"
#include "cpu/pic.h"
#include "cpu/task.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "multiboot.h"
#include "fs/tar.h"

uint32_t global_initrd_address = 0;

void kernel_main(uint32_t magic, uint32_t multiboot_info_addr)
{
    terminal_initialize();
    init_serial();
    serial_printf("\n\n=== IOTAOS KERNEL DEBUG SYSTEM STARTED ===\n");

    uint32_t bitmap_loc = 0x200000; /* Fallback to the 2MB mark */
    uint32_t total_memory = 256 * 1024 * 1024; /* Fallback to 256MB */

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        multiboot_info_t* mb_info = (multiboot_info_t*)multiboot_info_addr;
        
        /* Bit 0 of flags tells us if mem_upper and mem_lower are valid */
        if (mb_info->flags & 1) {
            /* mem_upper and mem_lower are provided in Kilobytes. 
               We add 1024 KB to account for the 1MB gap between them. */
            uint32_t total_kb = mb_info->mem_lower + mb_info->mem_upper + 1024;
            
            /* Prevent the 32-bit overflow if you give QEMU 4GB or more! */
            if (total_kb >= 4194304) {
                total_memory = 0xFFFFFFFF; /* Cap at exactly 4GB - 1 byte */
            } else {
                total_memory = total_kb * 1024;
            }
        }

        if (mb_info->mods_count > 0) {
            multiboot_module_t* module = (multiboot_module_t*)mb_info->mods_addr;
            global_initrd_address = module->mod_start;
            
            /* Dynamically place the PMM bitmap safely AFTER the ramdisk */
            bitmap_loc = (module->mod_end + 4095) & ~4095;
            
            tar_parse(global_initrd_address);
        } else {
            panic("Error: GRUB reports 0 modules loaded.\n");
        }
    } else {
        panic("Error: Magic number mismatch! Check boot.s pushes.\n");
    }
    
    pic_remap();
    idt_install();
    
    /* 1. PMM Initialization - using the dynamically detected RAM size! */
    pmm_init(total_memory, bitmap_loc); 

    /* Keep the kernel, initrd, and bitmap reserved before freeing allocatable RAM. */
    uint32_t bitmap_bytes = ((total_memory / 4096) + 7) / 8;
    uint32_t first_free = bitmap_loc + bitmap_bytes;
    first_free = (first_free + 4095) & ~4095;
    if (first_free < 0x400000) first_free = 0x400000;
    for (uint32_t i = first_free; i < (total_memory & ~4095); i += 4096) {
        pmm_mark_free(i);
    }

    paging_init(total_memory);

    /* 2. Initialize the Scheduler and spawn the background task */
    task_init();
    
    /* 3. Start the timer and enable interrupts */
    timer_init(100);
    __asm__ volatile("sti");

    char* IOTAOS_KERNEL_VERSION = "0.1.7";

    terminal_writestring("Hello, kernel World!\n");
    terminal_writestring("This is IotaOS, a simple 32-bit operating system kernel written in C.\n\n");

    printf(
        "  ___     _        ___  ____  \n"
        " |_ _|___ | |_ __ _ / _ \\/ ___| \n"
        "  | |/ _ \\| __/ _` | | | \\___ \\ \n"
        "  | | (_) | || (_| | |_| |___) |\n"
        " |___\\___/ \\__\\__,_|\\___/|____/ \n\n"
    );

    uint32_t sh_file_size = 0;
    void* sh_file = tar_get_file(global_initrd_address, "iosh.elf", &sh_file_size);
    if (sh_file) {
        int exit_code = elf_load_and_run(sh_file, sh_file_size);

        if (exit_code != 0) {
            char panic_msg[64];
            sprintf(panic_msg, "The Shell crashed unexpectedly (exit code %d)!", exit_code);
            panic(panic_msg);
        }

    } else {
        panic("Critical Error: iosh.elf not found in ramdisk!");
    }

    shutdown();

    printf("Welcome to IotaOS Shell (IOSH)!\n");

    while (1) {
        printf("user@IotaOS$ ");
        char cmd[256];
        gets(cmd, sizeof(cmd));
        
        if (strcmp(cmd, "help") == 0) {
            printf("Available commands:\n");
            printf("  help    - Show this help message\n");
            printf("  ls      - List files in the ramdisk\n");
            printf("  run     - Execute an .elf file (e.g. './test.elf')\n");
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
            char* filename = &cmd[2];
            
            if (global_initrd_address == 0) {
                printf("Error: No ramdisk loaded.\n");
            } else {
                uint32_t file_size = 0;
                void* file_data = tar_get_file(global_initrd_address, filename,
                                               &file_size);
                if (file_data == NULL) {
                    printf("Error: File '%s' not found.\n", filename);
                } else {
                    int exit_code = elf_load_and_run(file_data, file_size);
                    if (exit_code < 0) {
                        printf("Error: Failed to load or run '%s'.\n", filename);
                    }
                }
            }
        }
        else if (strcmp(cmd, "version") == 0) {
            printf("IotaOS Kernel Version: %s.\n", IOTAOS_KERNEL_VERSION);
            printf("Iota Shell (IOSH) Version: 0.1.0-beta.7\n");
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
            __asm__ volatile("int $1");
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