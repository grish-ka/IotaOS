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

uint32_t global_initrd_address = 0;

/* Programs init tries, in order, for the console session */
static const char* const login_programs[][2] = {
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
        } else {
            panic("Error: GRUB reports 0 modules loaded.\n");
        }
    } else {
        panic("Error: Magic number mismatch! Check boot.s pushes.\n");
    }
    
    /*
     * All of physical RAM is identity-mapped into every address space, so it
     * must stay below the per-process heap at TASK_HEAP_BASE (1 GB); memory
     * above that is not used.
     */
    if (total_memory > TASK_HEAP_BASE) total_memory = TASK_HEAP_BASE;

    pic_remap();
    idt_install();
    
    pmm_init(total_memory, bitmap_loc); 

    uint32_t bitmap_bytes = ((total_memory / 4096) + 7) / 8;
    uint32_t first_free = bitmap_loc + bitmap_bytes;
    first_free = (first_free + 4095) & ~4095;
    if (first_free < 0x400000) first_free = 0x400000;
    for (uint32_t i = first_free; i < (total_memory & ~4095); i += 4096) {
        /* Never hand out the frames behind the per-process program region:
           while a program runs, those addresses show its code and stack */
        if (i >= APP_REGION_BASE && i < APP_STACK_TOP) continue;
        pmm_mark_free(i);
    }

    paging_init(total_memory);
    task_init();
    timer_init(100);
    __asm__ volatile("sti");

    rtc_init();

    printf("IotaOS 1.0.0 (i686) - %d MB RAM\n\n", total_memory / 1024 / 1024);
    boot_ok("Initialized memory manager and paging");

    /* Root filesystem: the ramdisk unpacked into a writable in-memory tree */
    vfs_init();
    int entries = tar_load(global_initrd_address);
    char msg[80];
    sprintf(msg, "Mounted root filesystem (%d entries from initrd)", entries);
    boot_ok(msg);

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
    printf("\n");

    run_init();

    shutdown();
}