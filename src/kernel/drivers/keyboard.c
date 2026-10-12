/*
 * IotaOS - keyboard.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "keyboard.h"
#include "cpu/isr.h"
#include "drivers/system.h"
#include "drivers/tty.h"

static inline unsigned char kbd_inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

unsigned char kbd_uk[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   
    0, '#', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, 
    '*', 0,  ' ', 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Matches the hardcoded call inside your isr.c */
void keyboard_handle_interrupt(registers_t* regs) {
    (void)regs;
    
    unsigned char scancode = kbd_inb(0x60);
    
    if (!(scancode & 0x80)) {
        char ascii = 0;
        
        if (scancode == 0x56) {
            ascii = '\\';
        } else if (scancode < 128) {
            ascii = kbd_uk[scancode];
        }
        
        if (ascii != 0) {
            tty_input_char(ascii);
        }
    }
}

void init_keyboard(void) {
    /* Left empty because isr.c handles the routing directly */
}