/* 
 * IotaOS - terminal.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "terminal.h"
#include "drivers/string.h" /* Add this */

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000 

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;

/* --- ANSI STATE MACHINE --- */
#define ANSI_NORMAL 0
#define ANSI_ESC    1
#define ANSI_CSI    2

static int ansi_state = ANSI_NORMAL;
static int ansi_params[8];
static int ansi_param_count = 0;
static int ansi_current_param = 0;

/* Standard ANSI 30-37 map to standard VGA colors */
static const uint8_t ansi_to_vga[] = {
    VGA_COLOR_BLACK,      /* 30: Black */
    VGA_COLOR_RED,        /* 31: Red */
    VGA_COLOR_GREEN,      /* 32: Green */
    VGA_COLOR_BROWN,      /* 33: Yellow/Brown */
    VGA_COLOR_BLUE,       /* 34: Blue */
    VGA_COLOR_MAGENTA,    /* 35: Magenta */
    VGA_COLOR_CYAN,       /* 36: Cyan */
    VGA_COLOR_LIGHT_GREY  /* 37: White/Grey */
};

void terminal_clear(void) 
{
    terminal_row = 0;
    terminal_column = 0;
    if (terminal_color == 0) {
        terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
    
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_initialize(void) 
{
    terminal_clear();
}

void terminal_setcolor(uint8_t color) 
{
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

static void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t target_index = y * VGA_WIDTH + x;
            const size_t source_index = (y + 1) * VGA_WIDTH + x;
            terminal_buffer[target_index] = terminal_buffer[source_index];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
}

/* Helper to process raw character drawing */
static void terminal_putchar_raw(char c) 
{
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
        if (terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    } 
    else if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
        } else if (terminal_row > 0) {
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
        }
        terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
    } 
    else if (c == '\t') {
        terminal_column += 8 - (terminal_column % 8);
        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
            if (terminal_row == VGA_HEIGHT) {
                terminal_scroll();
                terminal_row = VGA_HEIGHT - 1;
            }
        }
    } 
    else {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        terminal_column++;
        if (terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
            if (terminal_row == VGA_HEIGHT) {
                terminal_scroll();
                terminal_row = VGA_HEIGHT - 1;
            }
        }
    }
}

/* Intercepts characters to parse ANSI escapes */
void terminal_putchar(char c) 
{
    if (ansi_state == ANSI_NORMAL) {
        if (c == '\033') { /* 0x1B ESC */
            ansi_state = ANSI_ESC;
        } else {
            terminal_putchar_raw(c);
        }
        return;
    } 
    
    if (ansi_state == ANSI_ESC) {
        if (c == '[') {
            ansi_state = ANSI_CSI;
            ansi_param_count = 0;
            ansi_current_param = 0;
            for(int i=0; i<8; i++) ansi_params[i] = 0;
        } else {
            /* Abort, not an ANSI CSI sequence */
            ansi_state = ANSI_NORMAL;
            terminal_putchar_raw(c);
        }
        return;
    } 
    
    if (ansi_state == ANSI_CSI) {
        if (c >= '0' && c <= '9') {
            ansi_current_param = (ansi_current_param * 10) + (c - '0');
        } 
        else if (c == ';') {
            if (ansi_param_count < 8) {
                ansi_params[ansi_param_count++] = ansi_current_param;
            }
            ansi_current_param = 0;
        } 
        else {
            /* Command letter reached */
            if (ansi_param_count < 8) {
                ansi_params[ansi_param_count++] = ansi_current_param;
            }

            if (c == 'm') { /* Colors (SGR) */
                if (ansi_param_count == 0 || (ansi_param_count == 1 && ansi_params[0] == 0)) {
                    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
                } else {
                    for (int i = 0; i < ansi_param_count; i++) {
                        int p = ansi_params[i];
                        if (p >= 30 && p <= 37) { /* Foreground */
                            uint8_t bg = terminal_color >> 4;
                            terminal_color = vga_entry_color(ansi_to_vga[p - 30], bg);
                        } else if (p >= 40 && p <= 47) { /* Background */
                            uint8_t fg = terminal_color & 0x0F;
                            terminal_color = vga_entry_color(fg, ansi_to_vga[p - 40]);
                        } else if (p == 0) { /* Reset */
                            terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
                        }
                    }
                }
            } 
            else if (c == 'H' || c == 'f') { /* Cursor Position (CUP) */
                int row = (ansi_param_count > 0 && ansi_params[0] > 0) ? ansi_params[0] - 1 : 0;
                int col = (ansi_param_count > 1 && ansi_params[1] > 0) ? ansi_params[1] - 1 : 0;
                if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
                if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
                terminal_row = row;
                terminal_column = col;
            } 
            else if (c == 'J') { /* Erase in Display (ED) */
                int mode = (ansi_param_count > 0) ? ansi_params[0] : 0;
                if (mode == 2) {
                    terminal_clear();
                }
            }
            
            ansi_state = ANSI_NORMAL;
        }
    }
}

void terminal_write(const char* data, size_t size) 
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) 
{
    terminal_write(data, strlen(data));
}