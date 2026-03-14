/*
* IotaOS - syscall.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "cpu/isr.h"
#include "drivers/stdio.h"
#include "drivers/terminal.h"
#include "drivers/system.h"
#include "mem/pmm.h"
#include "fs/tar.h"            /* Added for SYS_EXEC and SYS_LS */
#include "drivers/ib_loader.h" /* Added for SYS_EXEC */

/* We need to know where the ramdisk is in memory to load apps and list files */
extern uint32_t global_initrd_address;
int current_exit_code = 0; /* Global variable to store the exit code! */

void syscall_handler(registers_t *regs) {
    /* * Convention Reminder:
     * EAX: Syscall Number
     * EBX: Arg 1 | ECX: Arg 2 | EDX: Arg 3
     */

    switch (regs->eax) {
        case 0: /* SYS_PANIC */
            panic((char*)regs->ebx);
            break;

        case 1: /* SYS_PRINT */
            // Using ECX for the string pointer from C apps
            terminal_writestring((char*)regs->ecx); 
            break;

        case 2: /* SYS_REBOOT */
            reboot();
            break;

        case 3: /* SYS_READ_LINE */
            __asm__ volatile("sti"); // <--- UNLOCK THE CPU
            gets((char*)regs->ebx, (int)regs->ecx);
            __asm__ volatile("cli"); // <--- LOCK BACK BEFORE RETURNING
            break;

        case 4: /* SYS_CLEAR */
            terminal_clear();
            break;

        case 5: /* SYS_MEMINFO (For the 'meminfo' command) */
            // Return values go into EAX/EBX/ECX so the app can read them
            regs->eax = pmm_get_free_block_count();
            regs->ebx = pmm_get_total_block_count();
            break;

        case 6: /* SYS_EXIT */
            current_exit_code = regs->ebx; /* Save it so ib_loader can grab it! */
            printf("\n[Process exited with code %d]\n", current_exit_code);
            break;

        case 7: /* SYS_EXEC */
        {
            char* filename = (char*)regs->ebx;
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
            break;
        }

        case 8: /* SYS_LS */
            if (global_initrd_address == 0) {
                printf("Error: No ramdisk loaded.\n");
            } else {
                tar_parse(global_initrd_address);
            }
            break;

        default:
            printf("IotaOS: Unknown Syscall %d\n", regs->eax);
            break;
    }
}