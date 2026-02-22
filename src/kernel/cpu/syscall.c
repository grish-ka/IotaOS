/* * IotaOS - syscall.c
 * Copyright (c) 2026 grish-ka
 */

#include "cpu/isr.h"
#include "drivers/stdio.h"
#include "drivers/terminal.h"
#include "drivers/system.h"

void syscall_handler(registers_t *regs) {
    /* * WRONG: switch(regs->int_no)  <-- This would be 128
     * RIGHT: switch(regs->eax)     <-- This is your actual command
     */

    switch (regs->eax) {
        case 0: /* SYS_PANIC */
            panic((char*)regs->ebx);
            break;

        case 1: /* SYS_PRINT */
            terminal_writestring((char*)regs->ebx);
            break;

        case 2: /* SYS_REBOOT */
            reboot();
            break;

        default:
            /* If EAX is 0, but it falls through to here, 
               check if your registers_t struct is aligned correctly! */
            printf("IotaOS: Unknown Syscall %d (from Int %d)\n", regs->eax, regs->int_no);
            break;
    }
}