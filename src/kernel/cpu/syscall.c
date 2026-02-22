/* * IotaOS - syscall.c
 * Copyright (c) 2026 grish-ka
 */

#include "cpu/isr.h"
#include "drivers/stdio.h"
#include "drivers/terminal.h"
#include "drivers/system.h"

void syscall_handler(registers_t *regs) {
    switch (regs->eax) {
        case 0: /* SYS_PANIC */
            panic((char*)regs->ebx);
            break;

        case 1: /* SYS_PRINT */
            /* C App puts pointer in ECX. 
               We also need to add the base address where the app was loaded! */
            terminal_writestring((char*)regs->ecx); 
            break;

        case 2: /* SYS_REBOOT */
            reboot();
            break;
    }
}