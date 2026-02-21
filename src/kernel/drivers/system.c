/* 
 * IotaOS - system.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "system.h"
#include "io.h"
#include "stdio.h"
#include "terminal.h"

void panic(char* message) {

    /* Set our beautiful Blue Screen of Death colors! */
    terminal_setcolor(15 | 1 << 4); /* White text (15) on Blue (1) */
    terminal_clear();
    
    terminal_writestring("\n==================================================\n");
    terminal_writestring("                  KERNEL PANIC                      \n");
    terminal_writestring("==================================================\n");
    
    /* Dynamically print which error caused the crash! */
    terminal_writestring(" Exception: ");
    terminal_writestring(message);
    terminal_writestring("\n The system has been halted to prevent damage.\n");
    terminal_writestring("Press F1 to reboot...\n");
    
    /* --- HARDWARE POLLING FOR F1 --- */

    /* 1. Flush out any old garbage (like the Enter key you just pressed) */
    while (inb(0x64) & 1) {
        inb(0x60);
    }
    
    /* 2. Wait in an infinite loop until F1 (0x3B) is pressed */
    while (1) {
        /* If the keyboard controller says a key is ready... */
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60); /* Read the key */
            
            /* Check if it's the F1 key (0x3B) */
            if (scancode == 0x3B) {
                break; /* F1 was pressed! Break the loop and proceed to reboot */
            }
        }
    }

    /* Now we can safely reboot! */
    reboot();

}

void shutdown(void) {
    terminal_clear();

    printf("\n\n\n");
    printf("    ==================================================\n");
    printf("    |                                                |\n");
    printf("    |   It is now safe to turn off your computer.    |\n");
    printf("    |                                                |\n");
    printf("    ==================================================\n");
    
    /* cli: Clears (disables) interrupts so nothing wakes the CPU up.
       hlt: Halts the CPU until the next interrupt (which will never come).
    */
    __asm__ volatile("cli; hlt");
}

void reboot(void) {
    terminal_clear();

    printf("\nRebooting IotaOS...\n");
    
    /* Empty the keyboard buffer just to be safe */
    uint8_t temp = 0x02;
    while (temp & 0x02) {
        temp = inb(0x64);
    }
    
    /* Send the reset command to the keyboard controller! */
    outb(0x64, 0xFE);
    
    /* Halt the CPU just in case it takes a millisecond to trigger */
    __asm__ volatile("hlt");
}