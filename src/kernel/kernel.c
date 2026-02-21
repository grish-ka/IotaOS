#include "drivers/terminal.h"
#include "drivers/stdio.h"
#include "drivers/keyboard.h"
#include "drivers/string.h"
#include "drivers/system.h"
#include "cpu/idt.h"
#include "cpu/pic.h"

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This project needs to be compiled with a ix86-elf compiler"
#endif

void kernel_main(void) 
{
	/* Initialize terminal interface */
	terminal_initialize();

    pic_remap();

    /* Tell the CPU where the interrupt phonebook is! */
    idt_install();

    __asm__ volatile("sti");

	terminal_writestring("Hello, kernel World!\n");
    terminal_writestring("This is IotaOS, a simple 32-bit operating system kernel written in C.\n");
    terminal_writestring("It is designed to run on x86 hardware and serves as a starting point for learning about OS development.\n");

    printf("\n");

    printf(
        "  ___     _        ___  ____  \n"
        " |_ _|___ | |_ __ _ / _ \\/ ___| \n"
        "  | |/ _ \\| __/ _` | | | \\___ \\ \n"
        "  | | (_) | || (_| | |_| |___) |\n"
        " |___\\___/ \\__\\__,_|\\___/|____/ \n\n"
    );


    printf("sprintf test: %d, %x, %s, %c, %%\n", 12345, 0xABCD, "hello", 'A');

    /* 1. Create the empty bucket */
/* 1. Create the empty bucket */
    char test_buffer[100]; 

    /* 2. Fill the bucket using sprintf */
    sprintf(test_buffer, "sprintf test: %d, %x, %s, %c, %%\n", 12345, 0xABCD, "hello", 'A');

    /* 4. Check if the text inside the bucket matches what we expect! */
    const char* expected_output = "sprintf test: 12345, abcd, hello, A, %\n";
    
    if (strcmp(test_buffer, expected_output) == 0) {
        terminal_writestring("sprintf test passed!\n");
    } else {
        terminal_writestring("sprintf test failed!\n");
    }

    printf("\n");

    while (1) {
        printf("user@IotaOS$ ");
        char cmd[256];
        gets(cmd, sizeof(cmd));
        if (strcmp(cmd, "help") == 0) {
            printf("Available commands:\n");
            printf("  help    - Show this help message\n");
            printf("  test    - Run some basic tests to verify the kernel is working\n");
            printf("  version - Show the kernel and shell version\n");
            printf("  clear   - Clear the terminal screen\n");
            printf("  reboot  - Reboot the system\n");
            printf("  exit    - Exit the shell and shutdown the system\n");
        } 
        else if (strcmp(cmd, "test") == 0) {
            
            /* * Manually trigger Interrupt 1 (Debug) to test our IDT 
            * without the C compiler interfering! 
            */
            __asm__ volatile("int $1");
    
            printf("We will never reach this line!\n"); /* If we do, something went very wrong with our IDT setup! */

            printf("Isr test failed!\n"); /* Dead code ):*/
            
            printf("\n");
        }
        else if (strcmp(cmd, "version") == 0) {
            printf(
        "  ___ ___  ____  _   _ \n"
        " |_ _/ _ \\/ ___|| | | |\n"
        "  | | | | \\___ \\| |_| |\n"
        "  | | |_| |___) |  _  |\n"
        " |___\\___/|____/|_| |_|\n\n"
    );
            printf("IotaOS Kernel Version: 0.1.0-beta.4\n");
            printf("Iota Shell (IOSH) Version: 0.1.0\n");
        }
        else if (strcmp(cmd, "clear") == 0) {
            /* Resetting the terminal clears the screen naturally */
            terminal_clear();
        }
        else if (strcmp(cmd, "reboot") == 0) {
            reboot();
        }
        else if (strcmp(cmd, "exit") == 0) {
            shutdown();
        } else if (strcmp(cmd, "bsod") == 0) {

            /* Trigger a Blue Screen of Death (BSOD) by manually invoking an interrupt */
            /* * Manually trigger Interrupt 1 (Debug) to bsod our IDT 
            * without the C compiler interfering! 
            */
            __asm__ volatile("int $1");

        } else if (cmd[0] != '\0') {
            /* Only print "Unknown command" if they actually typed something */
            printf("IOSH: Unknown command: %s\n", cmd);
        }
    }

    shutdown();

}