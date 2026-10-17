/*
* IotaOS - isr.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "drivers/terminal.h"
#include "drivers/io.h"
#include "drivers/keyboard.h"
#include "drivers/system.h"
#include "isr.h"
#include "task.h"

/* A master array of error messages so we don't have to write 32 print statements */
char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)",
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)"
};

static void panic_write_hex(const char *label, uint32_t value) {
    const char *digits = "0123456789ABCDEF";
    char buf[11] = "0x";
    for (int i = 0; i < 8; i++) buf[2 + i] = digits[(value >> (28 - 4 * i)) & 0xF];
    buf[10] = '\0';
    terminal_writestring(label);
    terminal_writestring(buf);
    terminal_writestring("\n");
}

/*
 * Draw the blue screen for regs and wait for F1 to reboot. title names the
 * cause; err_code is shown as the error code (a CPU one, or one chosen by a
 * program through SYS_PANIC).
 */
void bsod(const char* title, uint32_t err_code, registers_t *regs) {
    terminal_setcolor(15 | 1 << 4); /* White text (15) on Blue (1) */
    terminal_clear();

    terminal_writestring("\n==================================================\n");
    terminal_writestring("                  KERNEL PANIC                      \n");
    terminal_writestring("==================================================\n");

    terminal_writestring(" Exception: ");
    terminal_writestring(title);
    terminal_writestring("\n");
    if (regs->int_no == 14) {
        uint32_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        panic_write_hex(" Fault address (CR2): ", cr2);
    }
    /* A GPF whose error code points into the IDT names the interrupt vector */
    if (regs->int_no == 13 && (err_code & 2)) {
        panic_write_hex(" IDT vector: ", err_code >> 3);
        terminal_writestring(err_code & 1 ? "  (raised by hardware)\n" : "  (raised by software)\n");
    }
    panic_write_hex(" EIP:        ", regs->eip);
    panic_write_hex(" Error code: ", err_code);
    panic_write_hex(" ESP:        ", regs->esp);
    panic_write_hex(" User ESP:   ", regs->useresp);
    terminal_writestring(" The system has been halted to prevent damage.\n");
    terminal_writestring("Press F1 to reboot...\n");

    /* --- HARDWARE POLLING FOR F1 --- */

    /* 1. Flush out any old garbage (like the Enter key you just pressed) */
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    /* 2. Wait until F1 (0x3B) is pressed */
    while (1) {
        if ((inb(0x64) & 1) && inb(0x60) == 0x3B) break;
    }

    reboot();
    for (;;) __asm__ volatile("cli; hlt");
}

/* The master function that all 32 assembly wrappers will call */
void fault_handler(registers_t *regs) {
    bsod(regs->int_no < 32 ? exception_messages[regs->int_no] : "Unknown Exception", regs->err_code, regs);
}

extern uint32_t timer_handler(uint32_t esp); /* <--- Updated signature */

/* This handles all hardware interrupts from the PIC */
uint32_t irq_handler(uint32_t esp) {
    /* Since ESP points perfectly to our pushed registers, we cast it! */
    registers_t *regs = (registers_t*)esp;

    /* If it is Interrupt 32 (IRQ 0), the timer caused it! */
    if (regs->int_no == 32) {
        esp = timer_handler(esp); /* <--- Catch the task switch! */
    }

    /* If it is Interrupt 33 (IRQ 1), the keyboard caused it! */
    if (regs->int_no == 33) {
        keyboard_handle_interrupt(regs);
        /* Ctrl-C may have just terminated the interrupted task */
        if (!task_current_active()) esp = task_schedule(esp);
    }

    /* task_yield(): a software interrupt, so no tick and no EOI */
    if (regs->int_no == 129) {
        return task_schedule(esp);
    }

    /*
     * Spurious IRQ 7 / IRQ 15: the PIC raises these for noise on the line,
     * even when masked. If the line is not really in service, it must not
     * get an end-of-interrupt (for IRQ 15 the master still needs one, for
     * the cascade on IRQ 2).
     */
    if (regs->int_no == 39 || regs->int_no == 47) {
        uint16_t pic = (regs->int_no == 39) ? 0x20 : 0xA0;
        outb(pic, 0x0B);                     /* OCW3: read the in-service register */
        if (!(inb(pic) & 0x80)) {
            if (regs->int_no == 47) outb(0x20, 0x20);
            return esp;
        }
    }

    /* Send the "End of Interrupt" (EOI) signal to the PIC.
       0x20 is the command, sent to port 0x20 (Master PIC). */
    if (regs->int_no >= 40) {
        outb(0xA0, 0x20); /* Send to Slave PIC too if necessary */
    }
    outb(0x20, 0x20);     /* Always send to Master PIC */

    return esp; /* <--- Return the new (or old) stack pointer to assembly */
}