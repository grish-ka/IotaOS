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

char keyboard_read(void) {
    if (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        if (scancode < 0x80) {
            char ascii = kbd_US[scancode];
            if (ascii != 0) {
                return ascii;
            }
        }
    }
    
    /* ADD THIS: If we get here, no valid key was pressed! */
    return 0; 
}

/* Waits for a keypress and returns the character */
char getchar(void) {
    char key = 0;
    while (key == 0) {
        key = keyboard_read();
    }
    
    /* A tiny, hacky delay so a single key press doesn't register 
       50 times in a row because modern CPUs are too fast! */
    for (volatile int i = 0; i < 100000; i++); 
    
    return key;
}