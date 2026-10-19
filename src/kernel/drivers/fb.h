/*
 * IotaOS - fb.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef _DRIVERS_FB_H
#define _DRIVERS_FB_H

#include <stdint.h>
#include <stdbool.h>
#include "multiboot.h"

typedef struct {
    uint32_t* address;      /* Linear framebuffer pointer (32-bit ARGB/XRGB) */
    uint32_t  paddr;        /* Physical address of framebuffer */
    uint32_t  pitch;        /* Bytes per scanline */
    uint32_t  width;        /* Pixels per row (e.g. 1024) */
    uint32_t  height;       /* Rows (e.g. 768) */
    uint8_t   bpp;          /* Bits per pixel (usually 32) */
    uint32_t  size;         /* Total framebuffer size in bytes */
    bool      enabled;      /* True if graphics mode is active */
} fb_info_t;

extern fb_info_t fb_info;

/* Core initialization from Multiboot */
void fb_init(multiboot_info_t* mb_info);

/* Drawing primitives */
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_getpixel(uint32_t x, uint32_t y);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_clear(uint32_t color);

/* Text rendering with 8x16 font */
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg);

/* Color helpers */
uint32_t fb_vga_to_rgb(uint8_t vga_color);

#endif
