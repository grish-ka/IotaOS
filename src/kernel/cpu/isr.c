#include "drivers/terminal.h"
#include "drivers/io.h"
#include "isr.h"

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
    "Reserved (If you are here you should be very scared or intel added a new CPU with more than 21 exceptions, make a github issue!)"
};

/* The master function that all 32 assembly wrappers will call */
void fault_handler(registers_t *regs) {
    /* Set our beautiful Blue Screen of Death colors! */
    terminal_setcolor(15 | 1 << 4); /* White text (15) on Blue (1) */
    terminal_clear();
    
    terminal_writestring("\n==================================================\n");
    terminal_writestring("                  KERNEL PANIC                      \n");
    terminal_writestring("==================================================\n");
    
    /* Dynamically print which error caused the crash! */
    terminal_writestring(" Exception: ");
    if (regs->int_no < 32) {
        terminal_writestring(exception_messages[regs->int_no]);
    } else {
        terminal_writestring("Unknown Exception");
    }
    terminal_writestring("\n The system has been halted to prevent damage.\n");
    
    /* Freeze the CPU forever */
    __asm__ volatile("cli; hlt");
}

/* We will write this in your keyboard.c next! */
extern void keyboard_handle_interrupt(void);

/* This handles all hardware interrupts from the PIC */
void irq_handler(registers_t *regs) {
    /* If it is Interrupt 33 (IRQ 1), the keyboard caused it! */
    if (regs->int_no == 33) {
        keyboard_handle_interrupt();
    }

    /* Send the "End of Interrupt" (EOI) signal to the PIC.
       0x20 is the command, sent to port 0x20 (Master PIC). */
    if (regs->int_no >= 40) {
        outb(0xA0, 0x20); /* Send to Slave PIC too if necessary */
    }
    outb(0x20, 0x20);     /* Always send to Master PIC */
}