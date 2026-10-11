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

/* UK QWERTY Scancode to ASCII map (Standard) */
char kbd_UK[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   
    0, '#', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
    '*', 0,  ' ', 0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

/* UK QWERTY Scancode to ASCII map (Shifted) */
char kbd_UK_shift[128] = {
    0,  27, '!', '"', '\x9C', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',   
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',   
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '@', '\xAA',   
    0, '~', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,
    '*', 0,  ' ', 0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

volatile char keyboard_buffer = 0;
static int shift_pressed = 0;

void keyboard_handle_interrupt(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    /* We inject the extra 105-key ISO key (next to left shift) dynamically */
    kbd_UK[0x56] = '\\';
    kbd_UK_shift[0x56] = '|';

    /* Left Shift (0x2A) or Right Shift (0x36) pressed */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    
    /* Left Shift (0xAA) or Right Shift (0xB6) released */
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }
    
    if (scancode < 0x80) {
        /* Pick the correct array based on the shift state */
        char ascii = shift_pressed ? kbd_UK_shift[scancode] : kbd_UK[scancode];
        if (ascii != 0) {
            keyboard_buffer = ascii;
        }
    }
}

char getchar(void) {
    char key = 0;
    while (keyboard_buffer == 0) {
        /* Halt the CPU to save power until the next interrupt wakes it up */
        __asm__ volatile("hlt"); 
    }
    key = keyboard_buffer;
    keyboard_buffer = 0;
    return key;
}