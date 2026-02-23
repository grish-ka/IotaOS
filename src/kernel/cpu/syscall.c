/* 
 * IotaOS - syscall.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

/* * IotaOS - syscall.c
 * Copyright (c) 2026 grish-ka
 */
#include "cpu/isr.h"
#include "drivers/stdio.h"
#include "drivers/terminal.h"
#include "drivers/system.h"
#include "mem/pmm.h"

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
            // For now, just print a return message. 
            // Later this will kill the process.
            printf("\n[Process exited with code %d]\n", regs->ebx);
            break;

        default:
            printf("IotaOS: Unknown Syscall %d\n", regs->eax);
            break;
    }
}