#include "drivers/terminal.h"
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