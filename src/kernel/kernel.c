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
#include "fs/file.h"
#include "cpu/syscall.h"
#include "drivers/rtc.h"
#include "drivers/devices.h"
#include "drivers/diag.h"
#include "drivers/fb.h"
#include "drivers/mouse.h"
#include "drivers/ata.h"

uint32_t global_initrd_address = 0;

/* Programs init tries, in order, for the console session */
static const char* const login_programs[][2] = {
    {"/bin/lightdm", "lightdm"},
    {"/bin/xfce", "xfce"},
    {"/bin/login", "login"},
    {"/bin/bash", "-bash"},
    {"/bin/iosh", "iosh"},
};

static void boot_ok(const char* msg) {
    printf("[  \033[32mOK\033[0m  ] %s\n", msg);
}

/* Task 0 becomes init: keep a login session running on the console forever */
static void run_init(void) {
    for (;;) {
        int pid = -1;
        /* No preemption while the child's task slot is half built */
        __asm__ volatile("cli");
        for (unsigned i = 0; i < sizeof(login_programs) / sizeof(login_programs[0]) && pid < 0; i++) {
            pid = spawn_program(login_programs[i][0], login_programs[i][1]);
        }
        if (pid >= 0) task_set_session_leader(pid);
        __asm__ volatile("sti");
        if (pid < 0) panic("init: no login program or shell found in /bin");

        int status;
        task_waitpid(pid, &status, 0);
    }
}

void kernel_main(uint32_t magic, uint32_t multiboot_info_addr)
{
    terminal_initialize();
    // init_serial();
    // serial_printf("\n\n=== IOTAOS KERNEL DEBUG SYSTEM STARTED ===\n");

    uint32_t bitmap_loc = 0x200000;
    uint64_t total_detected_bytes = 0;
    uint32_t total_memory = 256 * 1024 * 1024;
    uint32_t initrd_size = 0;
    uint32_t boot_flags = 0;
    bool has_mmap = false;

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        multiboot_info_t* mb_info = (multiboot_info_t*)multiboot_info_addr;
        fb_init(mb_info);
        if (fb_info.enabled) {
            terminal_initialize();
        }
        
        boot_flags = mb_info->flags;

        /* 1. Parse BIOS/UEFI E820 Memory Map (Multiboot flag bit 6) */
        if (mb_info->flags & MULTIBOOT_INFO_MEM_MAP) {
            has_mmap = true;
            uint32_t mmap_curr = mb_info->mmap_addr;
            uint32_t mmap_end = mb_info->mmap_addr + mb_info->mmap_length;
            uint32_t highest_usable_32bit = 0;

            while (mmap_curr < mmap_end) {
                multiboot_mmap_entry_t* entry = (multiboot_mmap_entry_t*)mmap_curr;
                if (entry->size == 0) break;

                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    total_detected_bytes += entry->len;

                    /* Track highest usable physical address below 4 GB */
                    if (entry->addr < 0x100000000ULL) {
                        uint64_t top = entry->addr + entry->len;
                        if (top > 0x100000000ULL) top = 0x100000000ULL;
                        if ((uint32_t)top > highest_usable_32bit) {
                            highest_usable_32bit = (uint32_t)top;
                        }
                    }
                }
                mmap_curr += entry->size + sizeof(entry->size);
            }

            if (highest_usable_32bit > 0) {
                total_memory = highest_usable_32bit;
            }
        }

        /* 2. Fallback to basic mem_lower + mem_upper (Multiboot flag bit 0) */
        if (!has_mmap && (mb_info->flags & MULTIBOOT_INFO_MEMORY)) {
            uint32_t total_kb = mb_info->mem_lower + mb_info->mem_upper + 1024;
            if (total_kb >= 4194304) {
                total_memory = 0xFFFFFFFF;
            } else {
                total_memory = total_kb * 1024;
            }
            total_detected_bytes = (uint64_t)total_memory;
        }

        if (total_detected_bytes == 0) {
            total_detected_bytes = (uint64_t)total_memory;
        }

        if (mb_info->mods_count > 0) {
            multiboot_module_t* module = (multiboot_module_t*)mb_info->mods_addr;
            global_initrd_address = module->mod_start;
            initrd_size = module->mod_end - module->mod_start;
            bitmap_loc = (module->mod_end + 4095) & ~4095;
        } else {
            panic("Error: GRUB reports 0 modules loaded.\n");
        }
    } else {
        panic("Error: Magic number mismatch! Check boot.s pushes.\n");
    }
    
    /*
     * All of physical RAM is identity-mapped into every address space, so it
     * must stay below the per-process heap at TASK_HEAP_BASE (2048 MB / 2 GB);
     * physical memory above TASK_HEAP_BASE is detected and reported.
     */
    if (total_memory > TASK_HEAP_BASE) total_memory = TASK_HEAP_BASE;

    pic_remap();
    idt_install();
    
    pmm_init(total_memory, bitmap_loc); 

    uint32_t bitmap_bytes = ((total_memory / 4096) + 7) / 8;
    uint32_t first_free = bitmap_loc + bitmap_bytes;
    first_free = (first_free + 4095) & ~4095;
    if (first_free < 0x400000) first_free = 0x400000;

    multiboot_info_t* mb_info = (multiboot_info_t*)multiboot_info_addr;
    if (has_mmap && (mb_info->flags & MULTIBOOT_INFO_MEM_MAP)) {
        /* Safely free only RAM certified as available in the E820 map */
        uint32_t mmap_curr = mb_info->mmap_addr;
        uint32_t mmap_end = mb_info->mmap_addr + mb_info->mmap_length;

        while (mmap_curr < mmap_end) {
            multiboot_mmap_entry_t* entry = (multiboot_mmap_entry_t*)mmap_curr;
            if (entry->size == 0) break;

            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint64_t start = entry->addr;
                uint64_t end = entry->addr + entry->len;

                if (start < first_free) start = first_free;
                if (end > total_memory) end = total_memory;

                if (start < end) {
                    for (uint32_t i = (uint32_t)start; i < (uint32_t)end; i += 4096) {
                        /* Never hand out the frames behind the per-process program region:
                           while a program runs, those addresses show its code and stack */
                        if (i >= APP_REGION_BASE && i < APP_STACK_TOP) continue;
                        pmm_mark_free(i);
                    }
                }
            }
            mmap_curr += entry->size + sizeof(entry->size);
        }
    } else {
        for (uint32_t i = first_free; i < (total_memory & ~4095); i += 4096) {
            if (i >= APP_REGION_BASE && i < APP_STACK_TOP) continue;
            pmm_mark_free(i);
        }
    }

    paging_init(total_memory);
    task_init();
    timer_init(100);
    boot_key_phase = 1;  /* a key held down from power-on counts as "diagnostics" */
    __asm__ volatile("sti");

    rtc_init();
    mouse_init();

    uint32_t detected_mb = (uint32_t)(total_detected_bytes / (1024 * 1024));
    uint32_t usable_mb = total_memory / (1024 * 1024);
    if (detected_mb > usable_mb) {
        printf("IotaOS 1.0.0 (i686) - %d MB RAM detected (%d MB active 32-bit)\n", detected_mb, usable_mb);
    } else {
        printf("IotaOS 1.0.0 (i686) - %d MB RAM\n", usable_mb);
    }
    if (fb_info.enabled) {
        printf("Display: VBE Linear Framebuffer %dx%d (%d bpp)\n\n", fb_info.width, fb_info.height, fb_info.bpp);
    } else {
        printf("Display: VGA text console (80x25)\n\n");
    }
    boot_ok("Initialized memory manager and paging");

    /* Root filesystem: the ramdisk unpacked into a writable in-memory tree */
    vfs_init();
    int entries = tar_load(global_initrd_address);
    char msg[80];
    sprintf(msg, "Mounted root filesystem (%d entries from initrd)", entries);
    boot_ok(msg);

    ata_init();
    vfs_node_t* console = devices_init();
    vfs_node_t* tmp = vfs_mkdir_p("/tmp");
    if (tmp) tmp->mode = 01777;
    vfs_mkdir_p("/root");
    vfs_mkdir_p("/home");
    boot_ok("Created device nodes in /dev");

    /* stdin, stdout and stderr of init (and everything it starts) are the console */
    fd_table_t* fds = task_get_fds();
    for (int fd = 0; fd < 3; fd++) {
        file_t* f = file_open_node(console, O_RDWR);
        fd_install(fds, f, fd);
    }
    boot_ok("Started console on /dev/tty1");

    /* A pause point on every boot: DEL (or Esc, F2) opens the diagnostics screen */
    int diag_requested = boot_key == BOOT_KEY_DEL || boot_key == BOOT_KEY_ESC || boot_key == BOOT_KEY_F2;
    if (!diag_requested) diag_requested = diag_boot_window(2);
    if (diag_requested) {
        diag_boot_info_t info = {
            .total_memory = total_memory,
            .detected_memory = total_detected_bytes,
            .initrd_address = global_initrd_address,
            .initrd_size = initrd_size,
            .fs_entries = (uint32_t)entries,
            .multiboot_flags = boot_flags,
        };
        diag_run(&info);
    }
    boot_key_phase = 0;
    terminal_clear();

    run_init();

    shutdown();
}