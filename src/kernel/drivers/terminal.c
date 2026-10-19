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
#include "drivers/audio.h"
#include "drivers/fb.h"

#define DEFAULT_FG VGA_COLOR_LIGHT_GREY
#define DEFAULT_BG VGA_COLOR_BLACK

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
static uint16_t fb_terminal_buffer[VGA_WIDTH * VGA_HEIGHT];
static uint16_t fb_rendered_buffer[VGA_WIDTH * VGA_HEIGHT];
uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;

/* --- ESCAPE SEQUENCE STATE MACHINE --- */
#define ANSI_NORMAL  0
#define ANSI_ESC     1   /* after ESC */
#define ANSI_CSI     2   /* after ESC [ */
#define ANSI_CHARSET 3   /* after ESC ( or ESC ): one designator byte follows */
#define ANSI_OSC     4   /* after ESC ] */
#define ANSI_SKIP    5   /* swallowing a fixed number of bytes */

#define MAX_PARAMS 8

static int ansi_state = ANSI_NORMAL;
static int ansi_params[MAX_PARAMS];
static int ansi_param_count = 0;
static int ansi_current_param = 0;
static int ansi_have_digit = 0;
static int ansi_private = 0;   /* CSI sequence started with '?' */
static int charset_target = 0; /* 0 = G0 "(", 1 = G1 ")" */
static int skip_count = 0;

/* SGR state: colours are recomputed from these on every change */
static uint8_t sgr_fg = DEFAULT_FG;
static uint8_t sgr_bg = DEFAULT_BG;
static int sgr_bold = 0;
static int sgr_reverse = 0;
static int sgr_underline = 0;

static size_t saved_row = 0;
static size_t saved_column = 0;

/* Scrolling region (inclusive rows), set with ESC [ top ; bottom r */
static size_t scroll_top = 0;
static size_t scroll_bottom = VGA_HEIGHT - 1;

/* Character sets: G0/G1 are either normal text or DEC line drawing */
static int g_graphics[2] = {0, 0};
static int active_g = 0;       /* SO (^N) selects G1, SI (^O) selects G0 */

static int insert_mode = 0;    /* ESC [ 4 h */

/* After writing the last column the cursor stays there until the next
   printable character (the "xn" quirk of the Linux console), so a line of
   exactly 80 characters followed by a newline does not leave a blank line */
static int wrap_pending = 0;

/* Where replies to status queries (cursor position etc.) are sent */
static void (*responder)(const char*) = 0;

void terminal_set_responder(void (*send)(const char*)) {
    responder = send;
}

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

/* DEC special graphics (line drawing) to code page 437, for '`' .. '~' */
static const uint8_t dec_graphics[] = {
    0x04, 0xB1, 0x09, 0x0C, 0x0D, 0x0A, 0xF8, 0xF1, /* ` a b c d e f g */
    0x0A, 0x0B, 0xD9, 0xBF, 0xDA, 0xC0, 0xC5, 0xC4, /* h i j k l m n o */
    0xC4, 0xC4, 0xC4, 0xC4, 0xC3, 0xB4, 0xC1, 0xC2, /* p q r s t u v w */
    0xB3, 0xF3, 0xF2, 0xE3, 0xD8, 0x9C, 0xFA,       /* x y z { | } ~   */
};

static void update_color(void) {
    uint8_t fg = sgr_fg;
    uint8_t bg = sgr_bg;
    /* VGA text mode cannot underline: like the Linux console, use a colour */
    if (sgr_underline && sgr_fg == DEFAULT_FG) fg = VGA_COLOR_CYAN;
    if (sgr_bold) fg |= 0x08; /* the Linux console shows bold as bright */
    if (sgr_reverse) {
        uint8_t t = fg;
        fg = bg;
        bg = t & 0x07;
    }
    terminal_color = vga_entry_color(fg, bg);
}

static void clear_cells(size_t from, size_t to) {
    if (from >= to) return;
    if (to > VGA_WIDTH * VGA_HEIGHT) to = VGA_WIDTH * VGA_HEIGHT;

    uint16_t entry = vga_entry(' ', terminal_color);

    if (fb_info.enabled && from == 0 && to >= VGA_WIDTH * VGA_HEIGHT) {
        fb_clear(fb_vga_to_rgb((terminal_color >> 4) & 0x0F));
        for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
            terminal_buffer[i] = entry;
            fb_rendered_buffer[i] = entry;
        }
        return;
    }

    for (size_t i = from; i < to; i++) {
        terminal_buffer[i] = entry;
        if (fb_info.enabled && fb_rendered_buffer[i] != entry) {
            fb_rendered_buffer[i] = entry;
            size_t x = i % VGA_WIDTH;
            size_t y = i / VGA_WIDTH;
            fb_draw_char(x * 8, y * 16, ' ', fb_vga_to_rgb(terminal_color & 0x0F),
                         fb_vga_to_rgb((terminal_color >> 4) & 0x0F));
        }
    }
}

void terminal_clear(void)
{
    wrap_pending = 0;
    terminal_row = 0;
    terminal_column = 0;
    if (terminal_color == 0) update_color();
    clear_cells(0, VGA_WIDTH * VGA_HEIGHT);
    if (fb_info.enabled) {
        fb_clear(fb_vga_to_rgb((terminal_color >> 4) & 0x0F));
        for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
            fb_rendered_buffer[i] = vga_entry(' ', terminal_color);
        }
    }
    terminal_flush_cursor();
}

static void reset_state(void) {
    sgr_fg = DEFAULT_FG;
    sgr_bg = DEFAULT_BG;
    sgr_bold = 0;
    sgr_reverse = 0;
    sgr_underline = 0;
    scroll_top = 0;
    scroll_bottom = VGA_HEIGHT - 1;
    g_graphics[0] = g_graphics[1] = 0;
    active_g = 0;
    insert_mode = 0;
    update_color();
}

void terminal_initialize(void)
{
    if (fb_info.enabled) {
        terminal_buffer = fb_terminal_buffer;
    } else {
        terminal_buffer = (uint16_t*)VGA_MEMORY;
    }
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        fb_rendered_buffer[i] = 0xFFFF;
    }
    reset_state();
    terminal_clear();
}

void terminal_setcolor(uint8_t color)
{
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    uint16_t entry = vga_entry(c, color);
    terminal_buffer[index] = entry;
    if (fb_info.enabled) {
        if (fb_rendered_buffer[index] != entry) {
            fb_rendered_buffer[index] = entry;
            uint8_t fg_idx = color & 0x0F;
            uint8_t bg_idx = (color >> 4) & 0x0F;
            fb_draw_char(x * 8, y * 16, c, fb_vga_to_rgb(fg_idx), fb_vga_to_rgb(bg_idx));
        }
    }
}

void terminal_flush_cursor(void) {
    if (!fb_info.enabled) {
        uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    } else {
        static size_t last_cur_x = 0, last_cur_y = 0;
        if (last_cur_y < VGA_HEIGHT && last_cur_x < VGA_WIDTH) {
            uint16_t entry = terminal_buffer[last_cur_y * VGA_WIDTH + last_cur_x];
            char c = (char)(entry & 0xFF);
            uint8_t color = (uint8_t)(entry >> 8);
            fb_draw_char(last_cur_x * 8, last_cur_y * 16, c,
                         fb_vga_to_rgb(color & 0x0F), fb_vga_to_rgb((color >> 4) & 0x0F));
            fb_rendered_buffer[last_cur_y * VGA_WIDTH + last_cur_x] = entry;
        }
        last_cur_x = terminal_column;
        last_cur_y = terminal_row;
        if (terminal_row < VGA_HEIGHT && terminal_column < VGA_WIDTH) {
            fb_fill_rect(terminal_column * 8, terminal_row * 16 + 14, 8, 2, 0x00FFFFFF);
            fb_rendered_buffer[terminal_row * VGA_WIDTH + terminal_column] = 0xFFFF;
        }
    }
}

/* Move rows [top, bottom] up by n lines, blanking the bottom ones */
static void scroll_up(size_t top, size_t bottom, size_t n) {
    if (n > bottom - top + 1) n = bottom - top + 1;
    for (size_t r = top; r + n <= bottom; r++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[r * VGA_WIDTH + x] = terminal_buffer[(r + n) * VGA_WIDTH + x];
        }
    }
    clear_cells((bottom + 1 - n) * VGA_WIDTH, (bottom + 1) * VGA_WIDTH);
    if (fb_info.enabled) {
        for (size_t r = top; r <= bottom; r++) {
            size_t row_idx = r * VGA_WIDTH;
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                size_t idx = row_idx + x;
                uint16_t entry = terminal_buffer[idx];
                if (entry != fb_rendered_buffer[idx]) {
                    fb_rendered_buffer[idx] = entry;
                    char c = (char)(entry & 0xFF);
                    uint8_t color = (uint8_t)(entry >> 8);
                    fb_draw_char(x * 8, r * 16, c,
                                 fb_vga_to_rgb(color & 0x0F), fb_vga_to_rgb((color >> 4) & 0x0F));
                }
            }
        }
    }
}

/* Move rows [top, bottom] down by n lines, blanking the top ones */
static void scroll_down(size_t top, size_t bottom, size_t n) {
    if (n > bottom - top + 1) n = bottom - top + 1;
    for (size_t r = bottom; r >= top + n; r--) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[r * VGA_WIDTH + x] = terminal_buffer[(r - n) * VGA_WIDTH + x];
        }
    }
    clear_cells(top * VGA_WIDTH, (top + n) * VGA_WIDTH);
    if (fb_info.enabled) {
        for (size_t r = top; r <= bottom; r++) {
            size_t row_idx = r * VGA_WIDTH;
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                size_t idx = row_idx + x;
                uint16_t entry = terminal_buffer[idx];
                if (entry != fb_rendered_buffer[idx]) {
                    fb_rendered_buffer[idx] = entry;
                    char c = (char)(entry & 0xFF);
                    uint8_t color = (uint8_t)(entry >> 8);
                    fb_draw_char(x * 8, r * 16, c,
                                 fb_vga_to_rgb(color & 0x0F), fb_vga_to_rgb((color >> 4) & 0x0F));
                }
            }
        }
    }
}

/* Line feed: down one row, scrolling the region at its bottom edge */
static void line_feed(void) {
    if (terminal_row == scroll_bottom) scroll_up(scroll_top, scroll_bottom, 1);
    else if (terminal_row < VGA_HEIGHT - 1) terminal_row++;
}

/* Reverse line feed (ESC M): up one row, scrolling at the top edge */
static void reverse_line_feed(void) {
    if (terminal_row == scroll_top) scroll_down(scroll_top, scroll_bottom, 1);
    else if (terminal_row > 0) terminal_row--;
}

/* Draw one character the way a terminal does: no newline translation */
static void put_raw(char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc >= 0x20 && wrap_pending) {
        wrap_pending = 0;
        terminal_column = 0;
        line_feed();
    }
    if (uc < 0x20) wrap_pending = 0;

    switch (uc) {
    case '\n':
    case '\v':
    case '\f':
        line_feed();
        return;
    case '\r':
        terminal_column = 0;
        return;
    case '\b':
        /* Cursor left only, as on a real terminal; erasing is "\b \b" */
        if (terminal_column > 0) terminal_column--;
        return;
    case '\t':
        terminal_column += 8 - (terminal_column % 8);
        if (terminal_column >= VGA_WIDTH) terminal_column = VGA_WIDTH - 1;
        return;
    case 0x0E: /* SO: select G1 */
        active_g = 1;
        return;
    case 0x0F: /* SI: select G0 */
        active_g = 0;
        return;
    }
    if (uc < 0x20) return; /* other control characters are not drawn */

    if (g_graphics[active_g] && uc >= '`' && uc <= '~') uc = dec_graphics[uc - '`'];

    size_t line = terminal_row * VGA_WIDTH;
    if (insert_mode) {
        for (size_t x = VGA_WIDTH - 1; x > terminal_column; x--) {
            terminal_buffer[line + x] = terminal_buffer[line + x - 1];
        }
        if (fb_info.enabled) {
            for (size_t x = terminal_column; x < VGA_WIDTH; x++) {
                uint16_t entry = terminal_buffer[line + x];
                fb_draw_char(x * 8, terminal_row * 16, (char)(entry & 0xFF),
                             fb_vga_to_rgb(entry >> 8 & 0x0F), fb_vga_to_rgb((entry >> 12) & 0x0F));
            }
        }
    }
    uint16_t entry = vga_entry(uc, terminal_color);
    terminal_buffer[line + terminal_column] = entry;
    if (fb_info.enabled) {
        fb_rendered_buffer[line + terminal_column] = entry;
        fb_draw_char(terminal_column * 8, terminal_row * 16, uc,
                     fb_vga_to_rgb(terminal_color & 0x0F),
                     fb_vga_to_rgb((terminal_color >> 4) & 0x0F));
    }
    if (terminal_column == VGA_WIDTH - 1) wrap_pending = 1;
    else terminal_column++;
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
            sgr_underline = 0;
        }
        else if (p == 1) sgr_bold = 1;
        else if (p == 22 || p == 2) sgr_bold = 0;
        else if (p == 4) sgr_underline = 1;
        else if (p == 24) sgr_underline = 0;
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

static void respond(const char* s) {
    if (responder) responder(s);
}

static void report_cursor(void) {
    char buf[16];
    int n = 0;
    buf[n++] = '\033';
    buf[n++] = '[';
    int row = terminal_row + 1, col = terminal_column + 1;
    if (row >= 10) buf[n++] = '0' + row / 10;
    buf[n++] = '0' + row % 10;
    buf[n++] = ';';
    if (col >= 10) buf[n++] = '0' + col / 10;
    buf[n++] = '0' + col % 10;
    buf[n++] = 'R';
    buf[n] = '\0';
    respond(buf);
}

static void run_csi(char c) {
    size_t here = terminal_row * VGA_WIDTH + terminal_column;
    size_t line = terminal_row * VGA_WIDTH;
    int n;

    if (c != 'm') wrap_pending = 0;
    if (ansi_private) {
        /* ESC[?25l/h (cursor), ?7l/h (autowrap), ?1c... : accepted, no effect */
        return;
    }

    switch (c) {
    case 'm':
        apply_sgr();
        break;
    case 'A':
        n = param(0, 1);
        terminal_row = (terminal_row >= (size_t)n) ? terminal_row - n : 0;
        break;
    case 'B':
    case 'e':
        n = param(0, 1);
        terminal_row = (terminal_row + n < VGA_HEIGHT) ? terminal_row + n : VGA_HEIGHT - 1;
        break;
    case 'C':
    case 'a':
        n = param(0, 1);
        terminal_column = (terminal_column + n < VGA_WIDTH) ? terminal_column + n : VGA_WIDTH - 1;
        break;
    case 'D':
        n = param(0, 1);
        terminal_column = (terminal_column >= (size_t)n) ? terminal_column - n : 0;
        break;
    case 'E':
        n = param(0, 1);
        terminal_row = (terminal_row + n < VGA_HEIGHT) ? terminal_row + n : VGA_HEIGHT - 1;
        terminal_column = 0;
        break;
    case 'F':
        n = param(0, 1);
        terminal_row = (terminal_row >= (size_t)n) ? terminal_row - n : 0;
        terminal_column = 0;
        break;
    case 'G':
    case '`':
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
        if (fb_info.enabled) {
            for (size_t x = terminal_column; x < VGA_WIDTH; x++) {
                uint16_t entry = terminal_buffer[line + x];
                fb_draw_char(x * 8, terminal_row * 16, (char)(entry & 0xFF),
                             fb_vga_to_rgb(entry >> 8 & 0x0F), fb_vga_to_rgb((entry >> 12) & 0x0F));
            }
        }
        break;
    case '@': /* insert blanks, shifting the rest of the line right */
        n = param(0, 1);
        for (size_t i = line + VGA_WIDTH; i-- > here;) {
            terminal_buffer[i] = (i >= here + n) ? terminal_buffer[i - n] : vga_entry(' ', terminal_color);
        }
        if (fb_info.enabled) {
            for (size_t x = terminal_column; x < VGA_WIDTH; x++) {
                uint16_t entry = terminal_buffer[line + x];
                fb_draw_char(x * 8, terminal_row * 16, (char)(entry & 0xFF),
                             fb_vga_to_rgb(entry >> 8 & 0x0F), fb_vga_to_rgb((entry >> 12) & 0x0F));
            }
        }
        break;
    case 'X': /* erase characters in place */
        n = param(0, 1);
        clear_cells(here, here + n < line + VGA_WIDTH ? here + n : line + VGA_WIDTH);
        break;
    case 'L': /* insert lines inside the scrolling region */
        if (terminal_row >= scroll_top && terminal_row <= scroll_bottom) {
            scroll_down(terminal_row, scroll_bottom, param(0, 1));
        }
        break;
    case 'M': /* delete lines inside the scrolling region */
        if (terminal_row >= scroll_top && terminal_row <= scroll_bottom) {
            scroll_up(terminal_row, scroll_bottom, param(0, 1));
        }
        break;
    case 'S':
        scroll_up(scroll_top, scroll_bottom, param(0, 1));
        break;
    case 'T':
        scroll_down(scroll_top, scroll_bottom, param(0, 1));
        break;
    case 'r': { /* set the scrolling region; the cursor goes home */
        int top = param(0, 1) - 1;
        int bottom = param(1, VGA_HEIGHT) - 1;
        if (bottom >= VGA_HEIGHT) bottom = VGA_HEIGHT - 1;
        if (top < bottom) {
            scroll_top = top;
            scroll_bottom = bottom;
        }
        terminal_row = 0;
        terminal_column = 0;
        break;
    }
    case 'h':
        if (param(0, 0) == 4) insert_mode = 1;
        break;
    case 'l':
        if (param(0, 0) == 4) insert_mode = 0;
        break;
    case 'n':
        if (param(0, 0) == 6) report_cursor();
        else if (param(0, 0) == 5) respond("\033[0n");
        break;
    case 'c':
        respond("\033[?6c"); /* "I am a VT102", as the Linux console says */
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

/* Feed one byte to the emulator, exactly as a terminal receives it */
void terminal_emit(char c)
{
    switch (ansi_state) {
    case ANSI_NORMAL:
        if (c == '\033') ansi_state = ANSI_ESC;
        else if (c == '\a') audio_bell(); /* PC speaker, no glyph */
        else put_raw(c);
        return;

    case ANSI_CHARSET:
        g_graphics[charset_target] = (c == '0');
        ansi_state = ANSI_NORMAL;
        return;

    case ANSI_SKIP:
        if (--skip_count <= 0) ansi_state = ANSI_NORMAL;
        return;

    case ANSI_OSC:
        /* Linux palette sequences: ESC ] R (reset) and ESC ] P nrrggbb;
           anything else runs to BEL or ESC \ */
        if (c == 'R') ansi_state = ANSI_NORMAL;
        else if (c == 'P') { ansi_state = ANSI_SKIP; skip_count = 7; }
        else if (c == '\a') ansi_state = ANSI_NORMAL;
        else if (c == '\033') ansi_state = ANSI_ESC;
        return;

    case ANSI_ESC:
        ansi_state = ANSI_NORMAL;
        switch (c) {
        case '[':
            ansi_state = ANSI_CSI;
            ansi_param_count = 0;
            ansi_current_param = 0;
            ansi_have_digit = 0;
            ansi_private = 0;
            for (int i = 0; i < MAX_PARAMS; i++) ansi_params[i] = 0;
            break;
        case '(':
        case ')':
            charset_target = (c == ')');
            ansi_state = ANSI_CHARSET;
            break;
        case ']':
            ansi_state = ANSI_OSC;
            break;
        case '7':
            saved_row = terminal_row;
            saved_column = terminal_column;
            break;
        case '8':
            terminal_row = saved_row;
            terminal_column = saved_column;
            wrap_pending = 0;
            break;
        case 'D':
            line_feed();
            break;
        case 'E':
            terminal_column = 0;
            line_feed();
            break;
        case 'M':
            reverse_line_feed();
            break;
        case 'c':
            reset_state();
            terminal_clear();
            break;
        case 'Z':
            respond("\033[?6c");
            break;
        /* ESC H (tab stop), ESC = / ESC > (keypad modes), ESC \ : ignored */
        }
        return;

    case ANSI_CSI:
        if (c >= '0' && c <= '9') {
            ansi_current_param = (ansi_current_param * 10) + (c - '0');
            ansi_have_digit = 1;
        } else if (c == ';') {
            if (ansi_param_count < MAX_PARAMS) ansi_params[ansi_param_count++] = ansi_current_param;
            ansi_current_param = 0;
            ansi_have_digit = 0;
        } else if (c == '?' || c == '>' || c == '=') {
            ansi_private = 1;
        } else if (c == '[') {
            /* ESC [ [ x: Linux function-key echo, swallow one byte */
            ansi_state = ANSI_SKIP;
            skip_count = 1;
        } else if (c >= 0x40 && c <= 0x7E) {
            if ((ansi_have_digit || ansi_param_count > 0) && ansi_param_count < MAX_PARAMS) {
                ansi_params[ansi_param_count++] = ansi_current_param;
            }
            run_csi(c);
            ansi_state = ANSI_NORMAL;
        }
        /* Intermediate bytes are ignored */
        return;
    }
}

/* Kernel output: '\n' starts a new line (carriage return + line feed) */
void terminal_putchar(char c)
{
    if (c == '\n' && ansi_state == ANSI_NORMAL) terminal_emit('\r');
    terminal_emit(c);
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
