#include "system.h"
#include "io.h"
#include "stdio.h"
#include "terminal.h"

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