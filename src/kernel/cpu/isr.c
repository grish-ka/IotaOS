#include "drivers/terminal.h"
#include "drivers/stdio.h"

/* This is the function called by our assembly wrapper */
void isr0_handler(void) {
    
    terminal_setcolor(VGA_COLOR_WHITE | VGA_COLOR_BLUE << 4);
    terminal_clear();
    /* Print a scary error message */
    terminal_writestring("\n==================================================\n");
    terminal_writestring("                  KERNEL PANIC                      \n");
    terminal_writestring("==================================================\n");
    terminal_writestring(" Exception: Divide by Zero (ISR 0)\n");
    terminal_writestring(" The system has been halted to prevent damage.\n");
    
    /* Freeze the CPU forever */
    __asm__ volatile("cli; hlt");
}
/* isr.c */
// void isr0_handler(void) {
//     /* 1. Point a pointer directly at the VGA text buffer */
//     char* video_memory = (char*) 0xB8000;
    
//     /* 2. Overwrite the top-left character on the screen */
//     video_memory[0] = 'E';    /* The character */
//     video_memory[1] = 0x4F;   /* The color: 4 = Red Background, F = White Text */
    
//     /* 3. Freeze the system */
//     __asm__ volatile("cli; hlt");
// }