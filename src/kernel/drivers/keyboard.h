/*
 * IotaOS - keyboard.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "cpu/isr.h"

void init_keyboard(void);
void keyboard_handle_interrupt(registers_t* regs);

#endif