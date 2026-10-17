/*
 * IotaOS - keyboard.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "keyboard.h"
#include "cpu/isr.h"
#include "drivers/system.h"
#include "drivers/tty.h"
#include "drivers/diag.h"

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

/* UK layout with Shift held (0 = no ASCII equivalent, e.g. the pound sign) */
unsigned char kbd_uk_shift[128] = {
    0,  27, '!', '"',  0,  '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '@',  0,
    0, '~', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,
    '*', 0,  ' ', 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Set 1 scancodes for modifier keys */
#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CTRL     0x1D
#define SC_CAPSLOCK 0x3A
#define SC_ISO_BSL  0x56  /* the extra UK key left of Z */
#define SC_EXTENDED 0xE0

static int shift_held = 0;
static int ctrl_held = 0;
static int caps_lock = 0;
static int extended = 0;

/* Extended (0xE0-prefixed) keys, sent as the ANSI sequences readline expects */
static void handle_extended(unsigned char scancode) {
    switch (scancode) {
    case 0x48: tty_input_seq("\x1b[A"); break;  /* Up */
    case 0x50: tty_input_seq("\x1b[B"); break;  /* Down */
    case 0x4D: tty_input_seq("\x1b[C"); break;  /* Right */
    case 0x4B: tty_input_seq("\x1b[D"); break;  /* Left */
    case 0x47: tty_input_seq("\x1b[H"); break;  /* Home */
    case 0x4F: tty_input_seq("\x1b[F"); break;  /* End */
    case 0x53: tty_input_seq("\x1b[3~"); break; /* Delete */
    case 0x52: tty_input_seq("\x1b[2~"); break; /* Insert */
    case 0x49: tty_input_seq("\x1b[5~"); break; /* Page Up */
    case 0x51: tty_input_seq("\x1b[6~"); break; /* Page Down */
    case 0x1C: tty_input_char('\n'); break;     /* Keypad Enter */
    case 0x35: tty_input_char('/'); break;      /* Keypad / */
    }
}

/* Matches the hardcoded call inside your isr.c */
void keyboard_handle_interrupt(registers_t* regs) {
    (void)regs;

    unsigned char scancode = kbd_inb(0x60);

    if (scancode == SC_EXTENDED) {
        extended = 1;
        return;
    }

    int released = scancode & 0x80;
    unsigned char key = scancode & 0x7F;

    /* Boot window: remember the key for the diagnostics prompt instead of typing it */
    if (boot_key_phase) {
        if (!released) boot_key = key;
        extended = 0;
        return;
    }

    if (extended) {
        extended = 0;
        if (key == SC_CTRL) ctrl_held = !released;  /* Right Ctrl */
        else if (!released) handle_extended(key);
        return;
    }

    if (key == SC_LSHIFT || key == SC_RSHIFT) { shift_held = !released; return; }
    if (key == SC_CTRL) { ctrl_held = !released; return; }
    if (released) return;
    if (key == SC_CAPSLOCK) { caps_lock = !caps_lock; return; }

    char ascii;
    if (key == SC_ISO_BSL) {
        ascii = shift_held ? '|' : '\\';
    } else {
        ascii = shift_held ? kbd_uk_shift[key] : kbd_uk[key];
        if (caps_lock && ascii >= 'a' && ascii <= 'z') ascii -= 32;
        else if (caps_lock && ascii >= 'A' && ascii <= 'Z') ascii += 32;
    }

    if (ctrl_held) {
        if ((ascii >= 'a' && ascii <= 'z') || (ascii >= 'A' && ascii <= 'Z')) ascii &= 0x1F;
        else return;
    }

    if (ascii != 0) {
        tty_input_char(ascii);
    }
}

void init_keyboard(void) {
    /* Left empty because isr.c handles the routing directly */
}