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
#include "drivers/tty.h"    
#include "cpu/idt.h"
#include "cpu/pic.h"
#include "cpu/task.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "multiboot.h"
#include "fs/tar.h"
#include "fs/vfs.h"

uint32_t global_initrd_address = 0;

vfs_node_t dev_root_node;
struct dirent dev_dirent;

static struct dirent *dev_readdir(vfs_node_t *node, uint32_t index) {
    (void)node;
    if (index == 0) {
        strcpy(dev_dirent.name, "tty");
        dev_dirent.ino = 0;
        return &dev_dirent;
    }
    return NULL;
}

static vfs_node_t *dev_finddir(vfs_node_t *node, char *name) {
    (void)node;
    if (strcmp(name, "tty") == 0) return tty_get_vfs_node();
    return NULL;
}

void kernel_main(uint32_t magic, uint32_t multiboot_info_addr)
{
    terminal_initialize();
    // init_serial();
    // serial_printf("\n\n=== IOTAOS KERNEL DEBUG SYSTEM STARTED ===\n");

    uint32_t bitmap_loc = 0x200000;
    uint32_t total_memory = 256 * 1024 * 1024;

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        multiboot_info_t* mb_info = (multiboot_info_t*)multiboot_info_addr;
        
        if (mb_info->flags & 1) {
            uint32_t total_kb = mb_info->mem_lower + mb_info->mem_upper + 1024;
            if (total_kb >= 4194304) {
                total_memory = 0xFFFFFFFF;
            } else {
                total_memory = total_kb * 1024;
            }
        }

        if (mb_info->mods_count > 0) {
            multiboot_module_t* module = (multiboot_module_t*)mb_info->mods_addr;
            global_initrd_address = module->mod_start;
            bitmap_loc = (module->mod_end + 4095) & ~4095;
            
            vfs_init();
            vfs_mount("/bin", tar_mount(global_initrd_address));
        } else {
            panic("Error: GRUB reports 0 modules loaded.\n");
        }
    } else {
        panic("Error: Magic number mismatch! Check boot.s pushes.\n");
    }
    
    /* Initialize TTY and mount the virtual /dev directory */
    tty_init();
    dev_root_node.flags = FS_DIRECTORY;
    dev_root_node.readdir = &dev_readdir;
    dev_root_node.finddir = &dev_finddir;
    vfs_mount("/dev", &dev_root_node);

    pic_remap();
    idt_install();
    
    pmm_init(total_memory, bitmap_loc); 

    uint32_t bitmap_bytes = ((total_memory / 4096) + 7) / 8;
    uint32_t first_free = bitmap_loc + bitmap_bytes;
    first_free = (first_free + 4095) & ~4095;
    if (first_free < 0x400000) first_free = 0x400000;
    for (uint32_t i = first_free; i < (total_memory & ~4095); i += 4096) {
        pmm_mark_free(i);
    }

    paging_init(total_memory);
    task_init();
    timer_init(100);
    __asm__ volatile("sti");

    terminal_writestring("Hello, kernel World!\n");
    terminal_writestring("This is IotaOS, a simple 32-bit operating system kernel written in C.\n\n");

    printf(
        "  ___     _        ___  ____  \n"
        " |_ _|___ | |_ __ _ / _ \\/ ___| \n"
        "  | |/ _ \\| __/ _` | | | \\___ \\ \n"
        "  | | (_) | || (_| | |_| |___) |\n"
        " |___\\___/ \\__\\__,_|\\___/|____/ \n\n"
    );

    vfs_node_t* sh_node = vfs_get_node("bin/iosh");
    if (sh_node) {
        uint32_t pages_needed = (sh_node->size + 4095) / 4096;
        uint8_t* temp_buffer = (uint8_t*)pmm_alloc_blocks(pages_needed);
        vfs_read(sh_node, 0, sh_node->size, temp_buffer);
        
        int exit_code = elf_load_and_run(temp_buffer, sh_node->size);

        for (uint32_t p = 0; p < pages_needed; p++) {
            pmm_free_block(temp_buffer + (p * 4096));
        }

        if (exit_code != 0) {
            char panic_msg[64];
            sprintf(panic_msg, "The Shell crashed unexpectedly (exit code %d)!", exit_code);
            panic(panic_msg);
        }
    } else {
        panic("Critical Error: bin/iosh not found in VFS!");
    }

    shutdown();
}