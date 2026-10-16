/*
 * IotaOS - terminal.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "terminal.h"
#include "drivers/string.h"
#include "drivers/io.h"

#define DEFAULT_FG VGA_COLOR_LIGHT_GREY
#define DEFAULT_BG VGA_COLOR_BLACK

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;

/* --- ANSI STATE MACHINE --- */
#define ANSI_NORMAL 0
#define ANSI_ESC    1
#define ANSI_CSI    2

#define MAX_PARAMS 8

static int ansi_state = ANSI_NORMAL;
static int ansi_params[MAX_PARAMS];
static int ansi_param_count = 0;
static int ansi_current_param = 0;
static int ansi_have_digit = 0;
static int ansi_private = 0;   /* CSI sequence started with '?' */

/* SGR state: colours are recomputed from these on every change */
static uint8_t sgr_fg = DEFAULT_FG;
static uint8_t sgr_bg = DEFAULT_BG;
static int sgr_bold = 0;
static int sgr_reverse = 0;

static size_t saved_row = 0;
static size_t saved_column = 0;

/* After writing the last column the cursor stays there until the next
   printable character (the "xn" quirk of the Linux console), so a line of
   exactly 80 characters followed by '\n' does not leave a blank line */
static int wrap_pending = 0;

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

static void update_color(void) {
    uint8_t fg = sgr_fg;
    uint8_t bg = sgr_bg;
    if (sgr_bold) fg |= 0x08; /* the Linux console shows bold as bright */
    if (sgr_reverse) {
        uint8_t t = fg;
        fg = bg;
        bg = t & 0x07;
    }
    terminal_color = vga_entry_color(fg, bg);
}

static void clear_cells(size_t from, size_t to) {
    for (size_t i = from; i < to && i < VGA_WIDTH * VGA_HEIGHT; i++) {
        terminal_buffer[i] = vga_entry(' ', terminal_color);
    }
}

void terminal_clear(void)
{
    wrap_pending = 0;
    terminal_row = 0;
    terminal_column = 0;
    if (terminal_color == 0) update_color();
    clear_cells(0, VGA_WIDTH * VGA_HEIGHT);
    terminal_flush_cursor();
}

void terminal_initialize(void)
{
    update_color();
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

void terminal_flush_cursor(void) {
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void terminal_scroll(void) {
    for (size_t i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
        terminal_buffer[i] = terminal_buffer[i + VGA_WIDTH];
    }
    clear_cells(VGA_WIDTH * (VGA_HEIGHT - 1), VGA_WIDTH * VGA_HEIGHT);
}

static void newline(void) {
    terminal_column = 0;
    terminal_row++;
    if (terminal_row == VGA_HEIGHT) {
        terminal_scroll();
        terminal_row = VGA_HEIGHT - 1;
    }
}

/* Helper to process raw character drawing */
static void terminal_putchar_raw(char c)
{
    if (c != '\n' && (unsigned char)c >= 0x20 && wrap_pending) {
        wrap_pending = 0;
        newline();
    }
    if ((unsigned char)c < 0x20) wrap_pending = 0;

    switch (c) {
    case '\n':
        newline();
        break;
    case '\r':
        terminal_column = 0;
        break;
    case '\b':
        /* Cursor left only, as on a real terminal; erasing is "\b \b" */
        if (terminal_column > 0) terminal_column--;
        break;
    case '\t':
        terminal_column += 8 - (terminal_column % 8);
        if (terminal_column >= VGA_WIDTH) newline();
        break;
    case '\a':
        break;
    default:
        if ((unsigned char)c < 0x20) break; /* other control characters are not drawn */
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (terminal_column == VGA_WIDTH - 1) wrap_pending = 1;
        else terminal_column++;
        break;
    }
}

static int param(int i, int def) {
    return (i < ansi_param_count && ansi_params[i] > 0) ? ansi_params[i] : def;
}

static void apply_sgr(void) {
    if (ansi_param_count == 0) {
        ansi_params[0] = 0;
        ansi_param_count = 1;
    }
    for (int i = 0; i < ansi_param_count; i++) {
        int p = ansi_params[i];
        if (p == 0) {
            sgr_fg = DEFAULT_FG;
            sgr_bg = DEFAULT_BG;
            sgr_bold = 0;
            sgr_reverse = 0;
        }
        else if (p == 1) sgr_bold = 1;
        else if (p == 22 || p == 2) sgr_bold = 0;
        else if (p == 7) sgr_reverse = 1;
        else if (p == 27) sgr_reverse = 0;
        else if (p >= 30 && p <= 37) sgr_fg = ansi_to_vga[p - 30];
        else if (p == 39) sgr_fg = DEFAULT_FG;
        else if (p >= 40 && p <= 47) sgr_bg = ansi_to_vga[p - 40];
        else if (p == 49) sgr_bg = DEFAULT_BG;
        else if (p >= 90 && p <= 97) sgr_fg = ansi_to_vga[p - 90] | 0x08;
        else if (p >= 100 && p <= 107) sgr_bg = ansi_to_vga[p - 100];
    }
    update_color();
}

static void run_csi(char c) {
    size_t here = terminal_row * VGA_WIDTH + terminal_column;
    size_t line = terminal_row * VGA_WIDTH;
    int n;

    if (ansi_private) return; /* e.g. ESC[?25l: cursor visibility, ignored */
    if (c != 'm') wrap_pending = 0;

    switch (c) {
    case 'm':
        apply_sgr();
        break;
    case 'A':
        n = param(0, 1);
        terminal_row = (terminal_row >= (size_t)n) ? terminal_row - n : 0;
        break;
    case 'B':
        n = param(0, 1);
        terminal_row = (terminal_row + n < VGA_HEIGHT) ? terminal_row + n : VGA_HEIGHT - 1;
        break;
    case 'C':
        n = param(0, 1);
        terminal_column = (terminal_column + n < VGA_WIDTH) ? terminal_column + n : VGA_WIDTH - 1;
        break;
    case 'D':
        n = param(0, 1);
        terminal_column = (terminal_column >= (size_t)n) ? terminal_column - n : 0;
        break;
    case 'G':
        n = param(0, 1);
        terminal_column = (n <= VGA_WIDTH) ? n - 1 : VGA_WIDTH - 1;
        break;
    case 'd':
        n = param(0, 1);
        terminal_row = (n <= VGA_HEIGHT) ? n - 1 : VGA_HEIGHT - 1;
        break;
    case 'H':
    case 'f': {
        int row = param(0, 1) - 1;
        int col = param(1, 1) - 1;
        terminal_row = (row < VGA_HEIGHT) ? row : VGA_HEIGHT - 1;
        terminal_column = (col < VGA_WIDTH) ? col : VGA_WIDTH - 1;
        break;
    }
    case 'J':
        n = (ansi_param_count > 0) ? ansi_params[0] : 0;
        if (n == 0) clear_cells(here, VGA_WIDTH * VGA_HEIGHT);
        else if (n == 1) clear_cells(0, here + 1);
        else clear_cells(0, VGA_WIDTH * VGA_HEIGHT);
        break;
    case 'K':
        n = (ansi_param_count > 0) ? ansi_params[0] : 0;
        if (n == 0) clear_cells(here, line + VGA_WIDTH);
        else if (n == 1) clear_cells(line, here + 1);
        else clear_cells(line, line + VGA_WIDTH);
        break;
    case 'P': /* delete characters, shifting the rest of the line left */
        n = param(0, 1);
        for (size_t i = here; i < line + VGA_WIDTH; i++) {
            terminal_buffer[i] = (i + n < line + VGA_WIDTH) ? terminal_buffer[i + n] : vga_entry(' ', terminal_color);
        }
        break;
    case '@': /* insert blanks, shifting the rest of the line right */
        n = param(0, 1);
        for (size_t i = line + VGA_WIDTH; i-- > here;) {
            terminal_buffer[i] = (i >= here + n) ? terminal_buffer[i - n] : vga_entry(' ', terminal_color);
        }
        break;
    case 'X': /* erase characters in place */
        clear_cells(here, here + param(0, 1) < line + VGA_WIDTH ? here + param(0, 1) : line + VGA_WIDTH);
        break;
    case 'L': /* insert lines */
        n = param(0, 1);
        for (size_t r = VGA_HEIGHT; r-- > terminal_row;) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_buffer[r * VGA_WIDTH + x] = (r >= terminal_row + n)
                    ? terminal_buffer[(r - n) * VGA_WIDTH + x] : vga_entry(' ', terminal_color);
            }
        }
        break;
    case 'M': /* delete lines */
        n = param(0, 1);
        for (size_t r = terminal_row; r < VGA_HEIGHT; r++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_buffer[r * VGA_WIDTH + x] = (r + n < VGA_HEIGHT)
                    ? terminal_buffer[(r + n) * VGA_WIDTH + x] : vga_entry(' ', terminal_color);
            }
        }
        break;
    case 's':
        saved_row = terminal_row;
        saved_column = terminal_column;
        break;
    case 'u':
        terminal_row = saved_row;
        terminal_column = saved_column;
        break;
    }
}

/* Intercepts characters to parse ANSI escapes */
void terminal_putchar(char c)
{
    if (ansi_state == ANSI_NORMAL) {
        if (c == '\033') ansi_state = ANSI_ESC;
        else terminal_putchar_raw(c);
        return;
    }

    if (ansi_state == ANSI_ESC) {
        ansi_state = ANSI_NORMAL;
        if (c == '[') {
            ansi_state = ANSI_CSI;
            ansi_param_count = 0;
            ansi_current_param = 0;
            ansi_have_digit = 0;
            ansi_private = 0;
            for (int i = 0; i < MAX_PARAMS; i++) ansi_params[i] = 0;
        } else if (c == '7') {
            saved_row = terminal_row;
            saved_column = terminal_column;
        } else if (c == '8') {
            terminal_row = saved_row;
            terminal_column = saved_column;
        } else if (c == 'c') {
            sgr_fg = DEFAULT_FG;
            sgr_bg = DEFAULT_BG;
            sgr_bold = 0;
            sgr_reverse = 0;
            update_color();
            terminal_clear();
        }
        /* Anything else (charset selection etc.) is ignored */
        return;
    }

    /* ANSI_CSI */
    if (c >= '0' && c <= '9') {
        ansi_current_param = (ansi_current_param * 10) + (c - '0');
        ansi_have_digit = 1;
    } else if (c == ';') {
        if (ansi_param_count < MAX_PARAMS) ansi_params[ansi_param_count++] = ansi_current_param;
        ansi_current_param = 0;
        ansi_have_digit = 0;
    } else if (c == '?' || c == '>' || c == '=') {
        ansi_private = 1;
    } else if (c >= 0x40 && c <= 0x7E) {
        if ((ansi_have_digit || ansi_param_count > 0) && ansi_param_count < MAX_PARAMS) {
            ansi_params[ansi_param_count++] = ansi_current_param;
        }
        run_csi(c);
        ansi_state = ANSI_NORMAL;
    }
    /* Intermediate bytes are ignored */
}

void terminal_write(const char* data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
    terminal_flush_cursor();
}

void terminal_writestring(const char* data)
{
    terminal_write(data, strlen(data));
}
