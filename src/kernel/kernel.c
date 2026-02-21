#include "drivers/terminal.h"

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

	terminal_writestring("Hello, kernel World!\n");
    terminal_writestring("This is IotaOS, a simple 32-bit operating system kernel written in C.\n");
    terminal_writestring("It is designed to run on x86 hardware and serves as a starting point for learning about OS development.\n");

    printf("Current version: %s\n", "0.1.0-beta.5");

    printf("sprintf test: %d, %x, %s, %c, %%\n", 12345, 0xABCD, "hello", 'A');

    if (sprintf("sprintf test: %d, %x, %s, %c\n", 12345, 0xABCD, "hello", 'A') == "sprintf test: 12345, abcd, hello, A,\n") {
        terminal_writestring("sprintf test passed!\n");
    } else {
        terminal_writestring("sprintf test failed!\n");
    }

}