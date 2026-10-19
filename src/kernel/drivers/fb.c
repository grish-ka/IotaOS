/*
* IotaOS - fb.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "drivers/fb.h"
#include "drivers/fb_font.h"
#include "drivers/stdio.h"
#include "drivers/string.h"

fb_info_t fb_info = {0};

/* Standard 16 VGA palette converted to 32-bit 0x00RRGGBB */
static const uint32_t vga_to_rgb_table[16] = {
    0x00000000, /* 0: Black */
    0x000000AA, /* 1: Blue */
    0x0000AA00, /* 2: Green */
    0x0000AAAA, /* 3: Cyan */
    0x00AA0000, /* 4: Red */
    0x00AA00AA, /* 5: Magenta */
    0x00AA5500, /* 6: Brown */
    0x00AAAAAA, /* 7: Light Grey */
    0x00555555, /* 8: Dark Grey */
    0x005555FF, /* 9: Light Blue */
    0x0055FF55, /* 10: Light Green */
    0x0055FFFF, /* 11: Light Cyan */
    0x00FF5555, /* 12: Light Red */
    0x00FF55FF, /* 13: Light Magenta */
    0x00FFFF55, /* 14: Yellow */
    0x00FFFFFF  /* 15: White */
};

uint32_t fb_vga_to_rgb(uint8_t vga_color) {
    return vga_to_rgb_table[vga_color & 0x0F];
}

void fb_init(multiboot_info_t* mb_info) {
    fb_info.enabled = false;
    fb_info.address = 0;

    if (!mb_info) return;

    /* Verify if GRUB provided modern linear framebuffer info (Multiboot 1 bit 12) */
    if ((mb_info->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) &&
        mb_info->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB &&
        (mb_info->framebuffer_bpp == 32 || mb_info->framebuffer_bpp == 24)) {
        fb_info.paddr = (uint32_t)mb_info->framebuffer_addr;
        fb_info.pitch = mb_info->framebuffer_pitch;
        fb_info.width = mb_info->framebuffer_width;
        fb_info.height = mb_info->framebuffer_height;
        fb_info.bpp = mb_info->framebuffer_bpp;
        fb_info.size = fb_info.pitch * fb_info.height;
        fb_info.address = (uint32_t*)fb_info.paddr;
        fb_info.enabled = true;
    }
    /* Fallback: Verify if GRUB provided VBE mode info (Multiboot 1 bit 11) */
    else if ((mb_info->flags & MULTIBOOT_INFO_VBE_INFO) && mb_info->vbe_mode_info) {
        vbe_mode_info_t* vbe = (vbe_mode_info_t*)mb_info->vbe_mode_info;
        if (vbe->physbase != 0 && (vbe->bpp == 32 || vbe->bpp == 24)) {
            fb_info.paddr = vbe->physbase;
            fb_info.pitch = vbe->pitch;
            fb_info.width = vbe->width;
            fb_info.height = vbe->height;
            fb_info.bpp = vbe->bpp;
            fb_info.size = fb_info.pitch * fb_info.height;
            fb_info.address = (uint32_t*)fb_info.paddr;
            fb_info.enabled = true;
        }
    }

    if (!fb_info.enabled) return;

    /* Clear screen to dark slate background on boot */
    fb_clear(0x00101820);
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_info.enabled || x >= fb_info.width || y >= fb_info.height) return;

    if (fb_info.bpp == 32) {
        uint32_t* pixel = (uint32_t*)((uint8_t*)fb_info.address + (y * fb_info.pitch) + (x * 4));
        *pixel = color;
    } else if (fb_info.bpp == 24) {
        uint8_t* p = (uint8_t*)fb_info.address + (y * fb_info.pitch) + (x * 3);
        p[0] = color & 0xFF;         /* Blue */
        p[1] = (color >> 8) & 0xFF;  /* Green */
        p[2] = (color >> 16) & 0xFF; /* Red */
    }
}

uint32_t fb_getpixel(uint32_t x, uint32_t y) {
    if (!fb_info.enabled || x >= fb_info.width || y >= fb_info.height) return 0;

    if (fb_info.bpp == 32) {
        uint32_t* pixel = (uint32_t*)((uint8_t*)fb_info.address + (y * fb_info.pitch) + (x * 4));
        return *pixel;
    } else if (fb_info.bpp == 24) {
        uint8_t* p = (uint8_t*)fb_info.address + (y * fb_info.pitch) + (x * 3);
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    }
    return 0;
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb_info.enabled || x >= fb_info.width || y >= fb_info.height) return;

    if (x + w > fb_info.width) w = fb_info.width - x;
    if (y + h > fb_info.height) h = fb_info.height - y;

    for (uint32_t r = 0; r < h; r++) {
        uint32_t py = y + r;
        if (fb_info.bpp == 32) {
            uint32_t* row = (uint32_t*)((uint8_t*)fb_info.address + (py * fb_info.pitch) + (x * 4));
            for (uint32_t c = 0; c < w; c++) row[c] = color;
        } else {
            for (uint32_t c = 0; c < w; c++) fb_putpixel(x + c, py, color);
        }
    }
}

void fb_clear(uint32_t color) {
    if (!fb_info.enabled) return;
    fb_fill_rect(0, 0, fb_info.width, fb_info.height, color);
}

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    if (!fb_info.enabled) return;
    if (x + 8 > fb_info.width || y + 16 > fb_info.height) return;

    uint8_t idx = (uint8_t)c;
    if (fb_info.bpp == 32) {
        uint8_t* row_base = (uint8_t*)fb_info.address + (y * fb_info.pitch) + (x * 4);
        for (uint32_t row = 0; row < 16; row++) {
            uint8_t bits = fb_font_8x16[idx][row];
            uint32_t* dst = (uint32_t*)(row_base + (row * fb_info.pitch));
            if (bg == 0xFFFFFFFF) {
                if (bits & 0x80) dst[0] = fg;
                if (bits & 0x40) dst[1] = fg;
                if (bits & 0x20) dst[2] = fg;
                if (bits & 0x10) dst[3] = fg;
                if (bits & 0x08) dst[4] = fg;
                if (bits & 0x04) dst[5] = fg;
                if (bits & 0x02) dst[6] = fg;
                if (bits & 0x01) dst[7] = fg;
            } else {
                dst[0] = (bits & 0x80) ? fg : bg;
                dst[1] = (bits & 0x40) ? fg : bg;
                dst[2] = (bits & 0x20) ? fg : bg;
                dst[3] = (bits & 0x10) ? fg : bg;
                dst[4] = (bits & 0x08) ? fg : bg;
                dst[5] = (bits & 0x04) ? fg : bg;
                dst[6] = (bits & 0x02) ? fg : bg;
                dst[7] = (bits & 0x01) ? fg : bg;
            }
        }
    } else {
        for (uint32_t row = 0; row < 16; row++) {
            uint8_t bits = fb_font_8x16[idx][row];
            for (uint32_t col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    fb_putpixel(x + col, y + row, fg);
                } else if (bg != 0xFFFFFFFF) {
                    fb_putpixel(x + col, y + row, bg);
                }
            }
        }
    }
}

void fb_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg) {
    if (!fb_info.enabled || !str) return;

    uint32_t cur_x = x;
    uint32_t cur_y = y;

    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += 16;
        } else if (*str == '\r') {
            cur_x = x;
        } else {
            fb_draw_char(cur_x, cur_y, *str, fg, bg);
            cur_x += 8;
            if (cur_x + 8 > fb_info.width) {
                cur_x = x;
                cur_y += 16;
            }
        }
        str++;
    }
}
