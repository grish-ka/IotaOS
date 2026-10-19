/*
 * IotaOS - mouse.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "drivers/mouse.h"
#include "drivers/io.h"
#include "drivers/fb.h"

static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static int32_t mouse_last_dx = 0;
static int32_t mouse_last_dy = 0;
static uint8_t mouse_buttons = 0;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_byte[3] = {0};

/* Mouse sensitivity: 100 = 100% Raw Direct Input (1:1 Tracking) */
static int mouse_sensitivity = 100;

/* Ring buffer for mouse input events */
#define MOUSE_QUEUE_SIZE 64
static mouse_event_t mouse_queue[MOUSE_QUEUE_SIZE];
static volatile uint32_t mouse_q_head = 0;
static volatile uint32_t mouse_q_tail = 0;

static void mouse_enqueue(int32_t x, int32_t y, int32_t buttons, int32_t wheel, int32_t dx, int32_t dy) {
    uint32_t next_head = (mouse_q_head + 1) % MOUSE_QUEUE_SIZE;
    if (next_head != mouse_q_tail) {
        mouse_queue[mouse_q_head].x = x;
        mouse_queue[mouse_q_head].y = y;
        mouse_queue[mouse_q_head].buttons = buttons;
        mouse_queue[mouse_q_head].wheel = wheel;
        mouse_queue[mouse_q_head].dx = dx;
        mouse_queue[mouse_q_head].dy = dy;
        mouse_q_head = next_head;
    }
}

int mouse_pop_event(mouse_event_t* ev) {
    if (mouse_q_head == mouse_q_tail) {
        return 0; /* Queue is empty */
    }
    if (ev) {
        *ev = mouse_queue[mouse_q_tail];
    }
    mouse_q_tail = (mouse_q_tail + 1) % MOUSE_QUEUE_SIZE;
    return 1;
}

static inline void mouse_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout-- && (inb(0x64) & 2)) {
        __asm__ volatile("pause");
    }
}

static inline void mouse_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout-- && !(inb(0x64) & 1)) {
        __asm__ volatile("pause");
    }
}

static void mouse_write(uint8_t byte) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, byte);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(0x60);
}

void mouse_init(void) {
    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags));

    if (fb_info.enabled && fb_info.width > 0 && fb_info.height > 0) {
        mouse_x = fb_info.width / 2;
        mouse_y = fb_info.height / 2;
    } else {
        mouse_x = 512;
        mouse_y = 384;
    }
    mouse_cycle = 0;
    mouse_buttons = 0;
    mouse_last_dx = 0;
    mouse_last_dy = 0;
    mouse_sensitivity = 100; /* 100% raw sensitivity */

    mouse_q_head = 0;
    mouse_q_tail = 0;
    /* Enqueue initial cursor position */
    mouse_enqueue(mouse_x, mouse_y, 0, 0, 0, 0);

    /* Flush output buffer */
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    /* Enable auxiliary device */
    mouse_wait_write();
    outb(0x64, 0xA8);

    /* Read Compaq status byte */
    mouse_wait_write();
    outb(0x64, 0x20);
    uint8_t status = mouse_read();

    /* Enable IRQ 12 and clear disable bit for mouse */
    status |= 0x02;
    status &= ~0x20;

    /* Write back Compaq status byte */
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    /* Reset to defaults */
    mouse_write(0xF6);
    mouse_read(); /* ACK 0xFA */

    /* Enable packet streaming */
    mouse_write(0xF4);
    mouse_read(); /* ACK 0xFA */

    /* Drain any pending bytes and reset packet cycle */
    while (inb(0x64) & 1) {
        inb(0x60);
    }
    mouse_cycle = 0;

    __asm__ volatile ("pushl %0; popfl" : : "r"(flags));
}

void mouse_handle_interrupt(void) {
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return;      /* No data ready */
    if (!(status & 0x20)) return;      /* Data is for keyboard, not mouse */

    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
    case 0:
        /* Bit 3 of byte 0 must be 1 in standard PS/2 packet */
        if (data & 0x08) {
            mouse_byte[0] = data;
            mouse_cycle = 1;
        }
        break;

    case 1:
        mouse_byte[1] = data;
        mouse_cycle = 2;
        break;

    case 2:
        mouse_byte[2] = data;
        mouse_cycle = 0;

        /* Check for overflow */
        if (mouse_byte[0] & 0xC0) break;

        /* Extract X delta */
        int32_t dx = (int32_t)mouse_byte[1];
        if (mouse_byte[0] & 0x10) {
            dx |= 0xFFFFFF00; /* Sign extend negative X */
        }

        /* Extract Y delta (PS/2 Y axis is inverted relative to screen) */
        int32_t dy = (int32_t)mouse_byte[2];
        if (mouse_byte[0] & 0x20) {
            dy |= 0xFFFFFF00; /* Sign extend negative Y */
        }

        /* 100% Raw Sensitivity (1:1 direct tracking without artificial damping) */
        int32_t eff_dx = (dx * mouse_sensitivity) / 100;
        int32_t eff_dy = (dy * mouse_sensitivity) / 100;

        mouse_last_dx = eff_dx;
        mouse_last_dy = eff_dy;

        mouse_x += eff_dx;
        mouse_y -= eff_dy; /* Screen Y points downwards */

        /* Clamp to display boundaries */
        uint32_t max_w = (fb_info.enabled && fb_info.width > 0) ? fb_info.width : 1024;
        uint32_t max_h = (fb_info.enabled && fb_info.height > 0) ? fb_info.height : 768;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= (int32_t)max_w) mouse_x = max_w - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= (int32_t)max_h) mouse_y = max_h - 1;

        mouse_buttons = mouse_byte[0] & 0x07;

        /* Push event into the ring buffer */
        mouse_enqueue(mouse_x, mouse_y, (int32_t)mouse_buttons, 0, eff_dx, eff_dy);
        break;
    }
}

void mouse_get_state(int32_t* x, int32_t* y, uint8_t* buttons) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
    if (buttons) *buttons = mouse_buttons;
}

void mouse_get_extended_state(mouse_state_t* st) {
    if (!st) return;
    st->x = mouse_x;
    st->y = mouse_y;
    st->buttons = (int32_t)mouse_buttons;
    st->wheel = 0;
    st->dx = mouse_last_dx;
    st->dy = mouse_last_dy;
}

void mouse_set_sensitivity(int scale_percent) {
    if (scale_percent < 20) scale_percent = 20;
    if (scale_percent > 300) scale_percent = 300;
    mouse_sensitivity = scale_percent;
}

void mouse_set_position(int32_t x, int32_t y) {
    uint32_t max_w = (fb_info.enabled && fb_info.width > 0) ? fb_info.width : 1024;
    uint32_t max_h = (fb_info.enabled && fb_info.height > 0) ? fb_info.height : 768;

    mouse_x = x;
    mouse_y = y;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= (int32_t)max_w) mouse_x = max_w - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= (int32_t)max_h) mouse_y = max_h - 1;

    mouse_enqueue(mouse_x, mouse_y, (int32_t)mouse_buttons, 0, 0, 0);
}

int mouse_is_vm(void) {
    return 0;
}
