/* 
 * IotaOS - keyboard.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "keyboard.h"
#include "io.h"
#include "stdio.h"
#include "terminal.h"
#include <stdint.h>

#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

/* US QWERTY Scancode to ASCII map */
const char kbd_US[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
    '*', 0,  ' ', 0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

/* A tiny "mailbox" to hold the last key typed */
volatile char keyboard_buffer = 0;

/* The interrupt automatically runs this exactly once per keypress! */
void keyboard_handle_interrupt(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    /* Only care about the "key pressed down" events (scancodes under 0x80) */
    if (scancode < 0x80) {
        char ascii = kbd_US[scancode];
        if (ascii != 0) {
            /* Put the letter in the mailbox! */
            keyboard_buffer = ascii;
        }
    }
}

/* Now getchar just checks the mailbox without locking up the hardware! */
char getchar(void) {
    char key = 0;
    
    /* Wait for the interrupt to put something in the mailbox */
    while (keyboard_buffer == 0) {
        /* We can put an assembly 'hlt' here later to save power while waiting! */
    }
    
    /* Grab the key and empty the mailbox */
    key = keyboard_buffer;
    keyboard_buffer = 0;
    
    return key;
}