/*
 * IotaOS - mouse.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
    int32_t buttons;
    int32_t wheel;
    int32_t dx;
    int32_t dy;
} __attribute__((packed)) mouse_event_t;

typedef mouse_event_t mouse_state_t;

void mouse_init(void);
void mouse_handle_interrupt(void);
void mouse_get_state(int32_t* x, int32_t* y, uint8_t* buttons);
void mouse_get_extended_state(mouse_state_t* st);
int mouse_pop_event(mouse_event_t* ev);
void mouse_set_sensitivity(int scale_percent);
void mouse_set_position(int32_t x, int32_t y);
int mouse_is_vm(void);

#endif /* MOUSE_H */
