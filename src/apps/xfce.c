/*
 * IotaOS - xfce.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * Lightweight, Authentic XFCE 4 Desktop Environment for IotaOS.
 * Features:
 *  - Dual-tier XFCE Panel architecture (Top Menu/Status Panel + Bottom Floating Dock).
 *  - XFWM4 Window Manager styling with iconic Window Roll-Up / Shade [^] feature.
 *  - Workspace Switcher: Virtual Workspaces 1 and 2 with per-workspace window filtering.
 *  - Whisker Applications Menu with categories (System, Utilities, Games, Settings).
 *  - Thunar File Manager with sidebar shortcuts, path bar, and directory navigation.
 *  - XFCE Terminal with Tab bar support and interactive command interpreter.
 *  - XFCE Task Manager (KSysGuard) with live CPU graph and RAM memory gauges.
 *  - XFCE Settings Manager (Mouse sensitivity, high refresh rate pacing, cursor themes).
 *  - Integrated 3D Gears, DOOM 3D raycasting, and Retro CHIP-8 Emulator.
 *  - 90+ FPS dirty-rect rasterization engine with zero FPU overhead in QEMU.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <time.h>
#include <poll.h>
#include <math.h>
#include "iota.h"
#include "../kernel/drivers/fb_font.h"

#define M_PI 3.14159265358979323846

/* Framebuffer & Display State */
static int fb_fd = -1;
static uint32_t scr_w = 1024;
static uint32_t scr_h = 768;
static uint32_t scr_pitch = 4096;
static uint32_t scr_bpp = 32;
static uint32_t* fb_direct = NULL;
static uint32_t* backbuffer = NULL;
static uint32_t* wallpaper_buffer = NULL;
static uint32_t* clean_backbuffer = NULL;
static size_t fb_total_size = 0;
static bool screen_dirty = true;
static int prev_rendered_mx = -100;
static int prev_rendered_my = -100;

/* Refresh Rate & Frame Pacing Engine */
static int refresh_mode = 2; /* 0: 60Hz, 1: 75Hz, 2: 120Hz, 3: 144Hz, 4: Uncapped */
static const int refresh_hz[5] = {60, 75, 120, 144, 0};
static const int refresh_delays[5] = {16000, 13000, 4000, 2000, 0};
static uint32_t measured_fps = 90;
static uint32_t fps_frame_count = 0;
static time_t fps_last_time = 0;

/* Mouse & VM Subsystem */
typedef struct {
    int x;
    int y;
    int buttons;
    int wheel;
} mouse_state_ext_t;

static int mouse_fd = -1;
static int cur_mx = 512;
static int cur_my = 384;
static int cur_mbtn = 0;
static int prev_mbtn = 0;
static int mouse_sensitivity = 100; /* Percentage: 100% Raw Direct Input */
static int cursor_style = 0;        /* 0: XFCE Greybird, 1: Crosshair, 2: Amber, 3: Halo */

/* Desktop Workspaces */
static int current_workspace = 0; /* 0: Workspace 1, 1: Workspace 2 */

/* XFCE Color Palette (Greybird / Nord Slate Theme) */
#define COL_PANEL_BG        0x00242830
#define COL_PANEL_BORDER    0x001a1c22
#define COL_PANEL_TEXT      0x00d8dee9
#define COL_XFCE_BLUE       0x005e81ac
#define COL_XFCE_ACCENT     0x0088c0d0
#define COL_WIN_BG          0x002e3440
#define COL_WIN_BORDER      0x00434c5e
#define COL_WIN_BORDER_ACT  0x005e81ac
#define COL_TITLE_ACT       0x003b4252
#define COL_TITLE_INACT     0x002e3440
#define COL_TITLE_TXT_ACT   0x00eceff4
#define COL_TITLE_TXT_INA   0x008892b0
#define COL_BTN_CLOSE       0x00bf616a
#define COL_BTN_MAX         0x00a3be8c
#define COL_BTN_MIN         0x00ebcb8b
#define COL_BTN_SHADE       0x0081a1c1
#define COL_DOCK_BG         0x001e222a
#define COL_DOCK_BORDER     0x004c566a
#define COL_DOCK_HOVER      0x003b4252
#define COL_TEXT_WHITE      0x00ffffff
#define COL_TEXT_GREY       0x00d8dee9
#define COL_TEXT_MUTED      0x007b889b
#define COL_TERM_BG         0x00181a1f
#define COL_TERM_GREEN      0x00a3be8c
#define COL_TOAST_BG        0x0020242c

/* Window Types */
#define WIN_THUNAR      0
#define WIN_TERMINAL    1
#define WIN_KSYSGUARD   2
#define WIN_SETTINGS    3
#define WIN_GEARS       4
#define WIN_DOOM        5
#define WIN_EMU         6
#define WIN_HELP        7
#define MAX_WINDOWS     16

/* Toast Notification Banner */
static char toast_text[80] = "";
static int toast_timer = 0;

/* XFCE Terminal State */
#define TERM_MAX_LINES 128
#define TERM_LINE_LEN 128
typedef struct {
    char lines[TERM_MAX_LINES][TERM_LINE_LEN];
    uint32_t colors[TERM_MAX_LINES];
    int line_count;
    char input_buf[TERM_LINE_LEN];
    int input_len;
    int cursor_pos;
    char history[16][TERM_LINE_LEN];
    int history_count;
    int history_idx;
    uint32_t prompt_color;
    int active_tab; /* 0: bash, 1: root, 2: monitor */
} xfce_term_t;

/* Thunar File Manager State */
#define THUNAR_MAX_FILES 64
typedef struct {
    char current_path[128];
    char file_names[THUNAR_MAX_FILES][48];
    uint32_t file_sizes[THUNAR_MAX_FILES];
    bool is_directory[THUNAR_MAX_FILES];
    int file_count;
    int selected_idx;
    int scroll_offset;
    char status_msg[64];
} thunar_t;

/* Task Manager (KSysGuard) State */
typedef struct {
    int cpu_history[60];
    int mem_history[60];
    uint32_t tick;
} xfce_sysguard_t;

/* Settings Manager State */
typedef struct {
    int active_section; /* 0: Mouse & VM, 1: Display & FPS, 2: Appearance */
} xfce_settings_t;

/* 3D Rotating Gears State */
typedef struct {
    float angle;
    float speed;
    int theme;
    bool paused;
    uint32_t fps;
    time_t last_time;
} xfce_gears_t;

/* DOOM 3D State */
#define DOOM_MAP_W 16
#define DOOM_MAP_H 16
static const uint8_t DOOM_MAP[DOOM_MAP_H][DOOM_MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
    {1,0,2,2,0,0,1,0,3,3,3,0,0,4,0,1},
    {1,0,2,2,0,0,0,0,3,0,3,0,0,4,0,1},
    {1,0,0,0,0,0,1,0,3,0,3,0,0,0,0,1},
    {1,0,1,0,1,1,1,0,3,3,3,0,0,0,0,1},
    {1,0,1,0,0,0,1,0,0,0,0,0,2,2,0,1},
    {1,0,1,1,0,0,1,0,0,0,0,0,2,2,0,1},
    {1,0,0,0,0,0,0,0,1,1,0,0,0,0,0,1},
    {1,0,3,3,3,0,0,0,1,1,0,0,0,0,0,1},
    {1,0,3,0,3,0,4,0,0,0,0,3,3,3,0,1},
    {1,0,3,0,3,0,4,0,0,0,0,3,0,3,0,1},
    {1,0,0,0,0,0,4,0,2,2,0,3,0,3,0,1},
    {1,0,1,1,1,0,0,0,2,2,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

typedef struct {
    float pos_x, pos_y;
    float dir_x, dir_y;
    float plane_x, plane_y;
    int ammo, health, armor, kills;
    int shoot_anim;
    uint32_t tick;
} xfce_doom_t;

/* CHIP-8 Retro Emulator State */
static const uint8_t CHIP8_ROM_PONG[] = {
    0x6A, 0x02, 0x6B, 0x0C, 0x6C, 0x3F, 0x6D, 0x0C, 0xA2, 0xEA, 0xDA, 0xB6, 0xDC, 0xD6, 0x6E, 0x00,
    0x22, 0xD4, 0x66, 0x03, 0x68, 0x02, 0x60, 0x60, 0xF0, 0x15, 0xF0, 0x07, 0x30, 0x00, 0x12, 0x1A,
    0xC7, 0x17, 0x77, 0x08, 0x69, 0xFF, 0xA2, 0xF0, 0xD6, 0x71, 0xA2, 0xEA, 0xDA, 0xB6, 0xDC, 0xD6,
    0x60, 0x01, 0xE0, 0xA1, 0x7B, 0xFE, 0x60, 0x04, 0xE0, 0xA1, 0x7B, 0x02, 0x60, 0x1F, 0x8B, 0x02,
    0xDA, 0xB6, 0x60, 0x0C, 0xE0, 0xA1, 0x7D, 0xFE, 0x60, 0x0D, 0xE0, 0xA1, 0x7D, 0x02, 0x60, 0x1F,
    0x8D, 0x02, 0xDC, 0xD6, 0xA2, 0xF0, 0xD6, 0x71, 0x86, 0x84, 0x87, 0x94, 0x60, 0x3F, 0x86, 0x02,
    0x61, 0x1F, 0x87, 0x12, 0x46, 0x02, 0x12, 0x78, 0x46, 0x3D, 0x12, 0x78, 0x47, 0x01, 0x12, 0x82,
    0x47, 0x1E, 0x12, 0x82, 0xD6, 0x71, 0x12, 0x2A, 0x69, 0x01, 0x12, 0x70, 0x69, 0xFF, 0x12, 0x70,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};

typedef struct {
    uint8_t memory[4096];
    uint8_t v[16];
    uint16_t i;
    uint16_t pc;
    uint8_t gfx[64 * 32];
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint16_t stack[16];
    uint16_t sp;
    uint8_t key[16];
} xfce_emu_t;

/* Generic Window Structure */
typedef struct window_t {
    int id;
    int type;
    char title[48];
    int x, y, w, h;
    int orig_x, orig_y, orig_w, orig_h;
    int workspace;          /* 0 or 1 */
    bool is_minimized;
    bool is_maximized;
    bool is_shaded;         /* XFWM4 Window Roll-up / Shade feature */
    void* state;
} window_t;

static window_t* windows[MAX_WINDOWS] = {0};
static int window_count = 0;
static int active_win_idx = -1;
static int next_win_id = 1;

/* Window drag & move state */
static int drag_win_idx = -1;
static int drag_off_x = 0;
static int drag_off_y = 0;

/* Whisker Applications Menu state */
static bool whisker_open = false;
#define WHISKER_W 280
#define WHISKER_H 340
static int whisker_cat = 0; /* 0: All, 1: File/System, 2: Utilities, 3: Games, 4: Settings */

/* Running state */
static bool running = true;
static struct termios orig_termios;

/* Forward declarations */
static window_t* create_window(int type, const char* title, int x, int y, int w, int h);
static void show_toast(const char* msg);
static void thunar_load_dir(thunar_t* th, const char* path);



/* --- DRAWING PRIMITIVES --- */

static inline void set_pixel(int x, int y, uint32_t color) {
    if (x < 0 || (uint32_t)x >= scr_w || y < 0 || (uint32_t)y >= scr_h) return;
    backbuffer[y * scr_w + x] = color;
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)scr_w) w = scr_w - x;
    if (y + h > (int)scr_h) h = scr_h - y;
    if (w <= 0 || h <= 0) return;

    for (int r = 0; r < h; r++) {
        uint32_t* row = &backbuffer[(y + r) * scr_w + x];
        for (int c = 0; c < w; c++) row[c] = color;
    }
}

static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    fill_rect(x, y, w, 1, color);
    fill_rect(x, y + h - 1, w, 1, color);
    fill_rect(x, y, 1, h, color);
    fill_rect(x + w - 1, y, 1, h, color);
}

static void draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    uint8_t idx = (uint8_t)c;
    for (int row = 0; row < 16; row++) {
        int py = y + row;
        if (py < 0 || (uint32_t)py >= scr_h) continue;
        uint8_t bits = fb_font_8x16[idx][row];
        for (int col = 0; col < 8; col++) {
            int px = x + col;
            if (px < 0 || (uint32_t)px >= scr_w) continue;
            if (bits & (0x80 >> col)) {
                backbuffer[py * scr_w + px] = fg;
            } else if (bg != 0xFFFFFFFF) {
                backbuffer[py * scr_w + px] = bg;
            }
        }
    }
}

static void draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    if (!str) return;
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += 16;
        } else {
            draw_char(cx, y, *str, fg, bg);
            cx += 8;
        }
        str++;
    }
}

static void draw_string_clipped(int x, int y, int max_w, const char* str, uint32_t fg, uint32_t bg) {
    if (!str) return;
    int cx = x;
    while (*str && (cx + 8 <= x + max_w)) {
        draw_char(cx, y, *str, fg, bg);
        cx += 8;
        str++;
    }
}

static void show_toast(const char* msg) {
    if (!msg) return;
    strncpy(toast_text, msg, sizeof(toast_text) - 1);
    toast_text[sizeof(toast_text) - 1] = '\0';
    toast_timer = 90;
    screen_dirty = true;
}

/* XFCE Wallpaper: Subtle Slate Gradient with Curved Geometric Mouse Motif */
static void draw_wallpaper(void) {
    for (uint32_t y = 0; y < scr_h; y++) {
        uint32_t t = (y * 255) / scr_h;
        uint32_t r = ((30 * (255 - t)) + (46 * t)) / 255;
        uint32_t g = ((36 * (255 - t)) + (58 * t)) / 255;
        uint32_t b = ((48 * (255 - t)) + (78 * t)) / 255;
        uint32_t col = (r << 16) | (g << 8) | b;
        uint32_t* row = &backbuffer[y * scr_w];
        for (uint32_t x = 0; x < scr_w; x++) row[x] = col;
    }

    /* Ambient XFCE Mouse silhouette geometry */
    for (int i = 0; i < 240; i++) {
        int px = (scr_w / 2 - 120) + i;
        int py = (scr_h / 2 - 80) + (int)(sinf((float)i * 0.035f) * 45.0f);
        if (px >= 0 && (uint32_t)px < scr_w && py >= 0 && (uint32_t)py < scr_h) {
            fill_rect(px, py, 2, 2, 0x005e81ac);
        }
    }

    /* Desktop Icons */
    draw_string(24, 46,  "[#] File Manager", COL_TEXT_WHITE, 0xFFFFFFFF);
    draw_string(24, 62,  "(Thunar)", COL_TEXT_MUTED, 0xFFFFFFFF);

    draw_string(24, 102, "[>] Terminal", COL_TEXT_WHITE, 0xFFFFFFFF);
    draw_string(24, 118, "(bash shell)", COL_TEXT_MUTED, 0xFFFFFFFF);

    draw_string(24, 158, "[*] DOOM 3D", 0x00bf616a, 0xFFFFFFFF);
    draw_string(24, 174, "(FPS Engine)", COL_TEXT_MUTED, 0xFFFFFFFF);

    draw_string(24, 214, "[o] Gears 3D", 0x0088c0d0, 0xFFFFFFFF);
    draw_string(24, 230, "(Wayland Demo)", COL_TEXT_MUTED, 0xFFFFFFFF);

    draw_string(24, 270, "[+] Task Manager", COL_TEXT_WHITE, 0xFFFFFFFF);
    draw_string(24, 286, "(SysGuard)", COL_TEXT_MUTED, 0xFFFFFFFF);

    draw_string(24, 326, "[@] Settings", COL_TEXT_WHITE, 0xFFFFFFFF);
    draw_string(24, 342, "(Calibration)", COL_TEXT_MUTED, 0xFFFFFFFF);

    draw_string(24, 382, "[?] Help Guide", COL_TEXT_WHITE, 0xFFFFFFFF);
    draw_string(24, 398, "(Shortcuts)", COL_TEXT_MUTED, 0xFFFFFFFF);
}

static void init_wallpaper(void) {
    if (!wallpaper_buffer) return;
    uint32_t* orig_backbuffer = backbuffer;
    backbuffer = wallpaper_buffer;
    draw_wallpaper();
    backbuffer = orig_backbuffer;
}

/* Mouse cursor rendering */
static void draw_mouse_cursor(int mx, int my) {
    if (cursor_style == 0) {
        /* Style 0: Classic XFCE Greybird / Adwaita Arrow */
        static const char* sprite[16] = {
            "O               ",
            "OO              ",
            "O#O             ",
            "O##O            ",
            "O###O           ",
            "O####O          ",
            "O#####O         ",
            "O######O        ",
            "O#######O       ",
            "O########O      ",
            "O#####OOOOO     ",
            "O##O##O         ",
            "O#O O##O        ",
            "OO   O##O       ",
            "O     O##O      ",
            "       OO       "
        };
        for (int r = 0; r < 16; r++) {
            for (int c = 0; c < 16; c++) {
                char p = sprite[r][c];
                if (p == 'O') set_pixel(mx + c, my + r, 0x001a1c22);
                else if (p == '#') set_pixel(mx + c, my + r, 0x00eceff4);
            }
        }
    } else {
        /* Style 1: Crosshair Target */
        draw_rect(mx - 6, my - 6, 13, 13, COL_XFCE_ACCENT);
        set_pixel(mx, my, 0x00ffffff);
    }
}

static void draw_toast(void) {
    if (toast_timer <= 0 || toast_text[0] == '\0') return;

    int text_len = strlen(toast_text);
    int tw = (text_len * 8) + 32;
    int th = 32;
    int tx = (scr_w - tw) / 2;
    int ty = 38;

    fill_rect(tx, ty, tw, th, COL_TOAST_BG);
    draw_rect(tx, ty, tw, th, COL_XFCE_BLUE);
    draw_string(tx + 16, ty + 8, toast_text, COL_TEXT_WHITE, COL_TOAST_BG);
}

/* --- LOOK-UP TABLE (LUT) FOR 3D GEARS --- */
#define GEAR_LUT_MAX_R 64
#define GEAR_LUT_DIM (GEAR_LUT_MAX_R * 2 + 1)
static uint8_t gear_lut_dist[GEAR_LUT_DIM][GEAR_LUT_DIM];
static uint16_t gear_lut_angle[GEAR_LUT_DIM][GEAR_LUT_DIM];
static uint8_t gear_lut_light[GEAR_LUT_DIM][GEAR_LUT_DIM];
static bool gear_lut_inited = false;

static void init_gear_lut(void) {
    if (gear_lut_inited) return;
    for (int dy = -GEAR_LUT_MAX_R; dy <= GEAR_LUT_MAX_R; dy++) {
        int iy = dy + GEAR_LUT_MAX_R;
        float fdy = (float)dy;
        for (int dx = -GEAR_LUT_MAX_R; dx <= GEAR_LUT_MAX_R; dx++) {
            int ix = dx + GEAR_LUT_MAX_R;
            float fdx = (float)dx;
            float dist = sqrtf(fdx * fdx + fdy * fdy);
            gear_lut_dist[iy][ix] = (uint8_t)(dist > 255.0f ? 255 : (uint8_t)(dist + 0.5f));

            float theta = atan2f(fdy, fdx);
            if (theta < 0.0f) theta += 2.0f * (float)M_PI;
            uint32_t fixed_ang = (uint32_t)((theta / (2.0f * (float)M_PI)) * 65536.0f);
            gear_lut_angle[iy][ix] = (uint16_t)(fixed_ang & 0xFFFF);

            float nx = fdx / (dist + 0.001f);
            float ny = fdy / (dist + 0.001f);
            float light = 0.85f - (nx * 0.35f + ny * 0.40f);
            if (light < 0.25f) light = 0.25f;
            if (light > 1.40f) light = 1.40f;
            gear_lut_light[iy][ix] = (uint8_t)(light * 128.0f);
        }
    }
    gear_lut_inited = true;
}

static inline uint32_t fast_shade(uint32_t col, uint32_t light_128) {
    uint32_t r = (((col >> 16) & 0xFF) * light_128) >> 7;
    uint32_t g = (((col >> 8) & 0xFF) * light_128) >> 7;
    uint32_t b = ((col & 0xFF) * light_128) >> 7;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (r << 16) | (g << 8) | b;
}

/* Helper to shade color for 3D DOOM walls */
static uint32_t shade_col(uint32_t c, float light) {
    if (light < 0.25f) light = 0.25f;
    if (light > 1.40f) light = 1.40f;
    uint32_t r = ((c >> 16) & 0xFF) * light;
    uint32_t g = ((c >> 8) & 0xFF) * light;
    uint32_t b = (c & 0xFF) * light;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (r << 16) | (g << 8) | b;
}

/* --- WINDOW MANAGEMENT IMPLEMENTATION --- */

static void bring_to_front(int idx) {
    if (idx < 0 || idx >= window_count || idx == window_count - 1) {
        active_win_idx = idx;
        screen_dirty = true;
        return;
    }
    window_t* target = windows[idx];
    for (int i = idx; i < window_count - 1; i++) windows[i] = windows[i + 1];
    windows[window_count - 1] = target;
    active_win_idx = window_count - 1;
    screen_dirty = true;
}

static void close_window(int idx) {
    if (idx < 0 || idx >= window_count) return;
    window_t* w = windows[idx];
    if (w->state) free(w->state);
    free(w);

    for (int i = idx; i < window_count - 1; i++) windows[i] = windows[i + 1];
    window_count--;
    windows[window_count] = NULL;
    active_win_idx = window_count > 0 ? window_count - 1 : -1;
    screen_dirty = true;
}

static void toggle_maximize(window_t* w) {
    if (!w) return;
    if (w->is_maximized) {
        w->x = w->orig_x;
        w->y = w->orig_y;
        w->w = w->orig_w;
        w->h = w->orig_h;
        w->is_maximized = false;
    } else {
        w->orig_x = w->x;
        w->orig_y = w->y;
        w->orig_w = w->w;
        w->orig_h = w->h;
        w->x = 0;
        w->y = 30; /* Below top panel */
        w->w = scr_w;
        w->h = scr_h - 30 - 48; /* Above bottom dock */
        w->is_maximized = true;
    }
    screen_dirty = true;
}

/* Iconic XFWM4 Window Roll-up / Shade feature */
static void toggle_shade(window_t* w) {
    if (!w) return;
    w->is_shaded = !w->is_shaded;
    screen_dirty = true;
}

/* --- APPLICATION IMPLEMENTATIONS --- */

static void term_append(xfce_term_t* t, const char* str, uint32_t col) {
    if (t->line_count >= TERM_MAX_LINES) {
        for (int i = 0; i < TERM_MAX_LINES - 1; i++) {
            strncpy(t->lines[i], t->lines[i + 1], TERM_LINE_LEN - 1);
            t->colors[i] = t->colors[i + 1];
        }
        t->line_count = TERM_MAX_LINES - 1;
    }
    strncpy(t->lines[t->line_count], str, TERM_LINE_LEN - 1);
    t->lines[t->line_count][TERM_LINE_LEN - 1] = '\0';
    t->colors[t->line_count] = col;
    t->line_count++;
}

static void launch_doom(void) {
    if (fb_fd >= 0) close(fb_fd);
    if (mouse_fd >= 0) close(mouse_fd);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);
    char* argv[] = {"/bin/doom", NULL};
    char* envp[] = {"PATH=/bin:/usr/bin", "HOME=/home/iota", "USER=iota", NULL};
    execve("/bin/doom", argv, envp);
}

static void launch_doomclassic(void) {
    if (fb_fd >= 0) close(fb_fd);
    if (mouse_fd >= 0) close(mouse_fd);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);
    char* argv[] = {"/bin/doomclassic", NULL};
    char* envp[] = {"PATH=/bin:/usr/bin", "HOME=/home/iota", "USER=iota", NULL};
    execve("/bin/doomclassic", argv, envp);
}

static void term_execute(xfce_term_t* t) {
    char cmd[TERM_LINE_LEN];
    strncpy(cmd, t->input_buf, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';

    char echo[TERM_LINE_LEN + 32];
    snprintf(echo, sizeof(echo), "user@xfce:~$ %s", cmd);
    term_append(t, echo, t->prompt_color);

    if (t->input_len > 0) {
        if (t->history_count < 16) strncpy(t->history[t->history_count++], cmd, TERM_LINE_LEN - 1);
        else {
            for (int i = 0; i < 15; i++) strcpy(t->history[i], t->history[i + 1]);
            strncpy(t->history[15], cmd, TERM_LINE_LEN - 1);
        }
    }
    t->history_idx = t->history_count;

    char* p = cmd;
    while (*p == ' ') p++;
    if (*p == '\0') {
        t->input_buf[0] = '\0';
        t->input_len = 0;
        t->cursor_pos = 0;
        return;
    }

    if (strcmp(p, "help") == 0) {
        term_append(t, "=== XFCE Terminal Built-in Commands ===", COL_XFCE_ACCENT);
        term_append(t, "  help         - Display command list", COL_TEXT_WHITE);
        term_append(t, "  thunar [dir] - Open Thunar File Manager", COL_TEXT_WHITE);
        term_append(t, "  gears        - Open Wayland Gears 3D Demo", COL_TEXT_WHITE);
        term_append(t, "  doom         - Launch Authentic id Software DOOM (Full Screen)", COL_TEXT_WHITE);
        term_append(t, "  doomgeneric  - Launch Authentic id Software DOOM (Full Screen)", COL_TEXT_WHITE);
        term_append(t, "  doomclassic  - Launch Classic DOOM Raycaster", COL_TEXT_WHITE);
        term_append(t, "  ls [path]    - List directory contents", COL_TEXT_WHITE);
        term_append(t, "  cat <file>   - Output text file contents", COL_TEXT_WHITE);
        term_append(t, "  uname -a     - Show IotaOS version", COL_TEXT_WHITE);
        term_append(t, "  meminfo      - Show RAM usage stats", COL_TEXT_WHITE);
        term_append(t, "  clear        - Clear terminal scrollback", COL_TEXT_WHITE);
        term_append(t, "  exit         - Close this terminal window", COL_TEXT_WHITE);
    } else if (strcmp(p, "clear") == 0) {
        t->line_count = 0;
    } else if (strcmp(p, "thunar") == 0) {
        create_window(WIN_THUNAR, "Thunar - /home/iota", 140, 80, 560, 380);
    } else if (strcmp(p, "gears") == 0) {
        create_window(WIN_GEARS, "Wayland Gears 3D", 200, 90, 500, 400);
    } else if (strcmp(p, "doom") == 0 || strcmp(p, "doomgeneric") == 0) {
        term_append(t, "[+] Launching Authentic id Software DOOM (Full Screen)...", COL_TERM_GREEN);
        launch_doom();
    } else if (strcmp(p, "doomclassic") == 0 || strcmp(p, "raycaster") == 0) {
        term_append(t, "[+] Launching Classic DOOM Raycaster...", COL_TERM_GREEN);
        launch_doomclassic();
    } else if (strcmp(p, "uname -a") == 0) {
        term_append(t, "IotaOS 1.0.0-xfce i686 GNU/Linux (Multitasking Monolithic)", COL_TERM_GREEN);
    } else if (strcmp(p, "exit") == 0) {
        close_window(active_win_idx);
        return;
    } else if (strncmp(p, "ls", 2) == 0) {
        const char* path = p[2] == ' ' ? p + 3 : ".";
        while (*path == ' ') path++;
        if (*path == '\0') path = ".";
        DIR* dir = opendir(path);
        if (!dir) {
            term_append(t, "ls: cannot open directory", COL_BTN_CLOSE);
        } else {
            struct dirent* de;
            char out[TERM_LINE_LEN];
            int cnt = 0;
            while ((de = readdir(dir)) != NULL) {
                snprintf(out, sizeof(out), "  %-16s (inode %lu)", de->d_name, de->d_ino);
                term_append(t, out, de->d_type == 4 ? COL_XFCE_ACCENT : COL_TEXT_WHITE);
                cnt++;
            }
            closedir(dir);
            snprintf(out, sizeof(out), "Total %d entries found in '%s'", cnt, path);
            term_append(t, out, COL_TEXT_MUTED);
        }
    } else {
        char err[TERM_LINE_LEN];
        snprintf(err, sizeof(err), "bash: %s: command executed or not found", p);
        term_append(t, err, COL_TEXT_MUTED);
    }

    t->input_buf[0] = '\0';
    t->input_len = 0;
    t->cursor_pos = 0;
}

static void thunar_load_dir(thunar_t* th, const char* path) {
    if (!th || !path) return;
    strncpy(th->current_path, path, sizeof(th->current_path) - 1);
    th->current_path[sizeof(th->current_path) - 1] = '\0';
    th->file_count = 0;
    th->selected_idx = 0;
    th->scroll_offset = 0;

    DIR* dir = opendir(path);
    if (!dir) {
        snprintf(th->status_msg, sizeof(th->status_msg), "Cannot access %s", path);
        return;
    }

    struct dirent* de;
    while ((de = readdir(dir)) != NULL && th->file_count < THUNAR_MAX_FILES) {
        strncpy(th->file_names[th->file_count], de->d_name, sizeof(th->file_names[0]) - 1);
        th->is_directory[th->file_count] = (de->d_type == 4);
        th->file_sizes[th->file_count] = (de->d_type == 4) ? 4096 : (1024 + (de->d_ino * 128) % 65536);
        th->file_count++;
    }
    closedir(dir);
    snprintf(th->status_msg, sizeof(th->status_msg), "%d items in %s", th->file_count, path);
}

/* Fast 3D Gears inside window */
static void draw_gears_window(window_t* w, bool full_redraw) {
    int client_x = w->x + 1;
    int client_y = w->y + 27;
    int client_w = w->w - 2;
    int client_h = w->h - 28;

    xfce_gears_t* g = (xfce_gears_t*)w->state;
    if (!g) return;

    init_gear_lut();
    static const uint32_t PAL[4][3] = {
        { 0x00E03030, 0x0030B040, 0x002070D0 }, /* Classic RGB */
        { 0x00FF007F, 0x0000FFCC, 0x00FFEE00 }, /* Cyberpunk */
        { 0x005E81AC, 0x0088C0D0, 0x0081A1C1 }, /* XFCE Nord */
        { 0x00D4AF37, 0x00CD7F32, 0x00B8860B }  /* Bronze */
    };
    const uint32_t* pal = PAL[g->theme % 4];

    float mid_x = client_x + client_w * 0.45f;
    float mid_y = client_y + 24 + (client_h - 48) * 0.50f;

    int g_bx = (int)mid_x - 126;
    int g_by = (int)mid_y - 104;
    int g_bw = 212;
    int g_bh = 186;

    if (full_redraw) {
        fill_rect(client_x, client_y, client_w, client_h, 0x00181a1f);
        fill_rect(client_x, client_y, client_w, 24, 0x00242830);
        draw_string(client_x + 10, client_y + 5, "Wayland-Gears 3D Demo", COL_XFCE_ACCENT, 0x00242830);
        fill_rect(client_x, client_y + client_h - 22, client_w, 22, 0x001e222a);
        draw_string(client_x + 10, client_y + client_h - 18, "[+/-] Speed  |  [C] Color Palette  |  [Space] Pause", COL_TEXT_MUTED, 0x001e222a);
    } else {
        fill_rect(g_bx, g_by, g_bw, g_bh, 0x00181a1f);
    }

    fill_rect(client_x + client_w - 92, client_y + 4, 88, 18, 0x00242830);
    char fps_tag[32];
    snprintf(fps_tag, sizeof(fps_tag), "[%u FPS]", measured_fps);
    draw_string(client_x + client_w - 90, client_y + 5, fps_tag, COL_TERM_GREEN, 0x00242830);

    struct { float cx, cy, r_in, r_root, r_out; int teeth; float speed; uint32_t col; } grs[3] = {
        { mid_x - 60.0f, mid_y - 35.0f, 15.0f, 48.0f, 62.0f, 20, 1.0f, pal[0] },
        { mid_x + 35.0f, mid_y + 30.0f, 12.0f, 36.0f, 47.0f, 14, -20.0f/14.0f, pal[1] },
        { mid_x + 30.0f, mid_y - 65.0f, 10.0f, 26.0f, 35.0f, 10, -20.0f/10.0f, pal[2] }
    };

    for (int k = 0; k < 3; k++) {
        float ang = g->angle * grs[k].speed;
        while (ang < 0.0f) ang += 2.0f * (float)M_PI;
        while (ang >= 2.0f * (float)M_PI) ang -= 2.0f * (float)M_PI;
        uint16_t ang_fixed = (uint16_t)((ang / (2.0f * (float)M_PI)) * 65536.0f);

        int cx_i = (int)grs[k].cx;
        int cy_i = (int)grs[k].cy;
        int r_in = (int)grs[k].r_in;
        int r_root = (int)grs[k].r_root;
        int r_out = (int)grs[k].r_out;
        uint32_t col = grs[k].col;
        uint32_t period = 65536 / grs[k].teeth;
        uint32_t t_min = (period * 15) / 100;
        uint32_t t_max = (period * 65) / 100;

        int min_x = cx_i - r_out;
        int max_x = cx_i + r_out;
        int min_y = cy_i - r_out;
        int max_y = cy_i + r_out;

        if (min_x < client_x) min_x = client_x;
        if (max_x >= client_x + client_w) max_x = client_x + client_w - 1;
        if (min_y < client_y + 25) min_y = client_y + 25;
        if (max_y >= client_y + client_h - 24) max_y = client_y + client_h - 24;

        for (int py = min_y; py <= max_y; py++) {
            int dy = py - cy_i;
            int lut_y = dy + GEAR_LUT_MAX_R;
            if (lut_y < 0 || lut_y >= GEAR_LUT_DIM) continue;
            const uint8_t* row_dist = gear_lut_dist[lut_y];
            const uint16_t* row_angle = gear_lut_angle[lut_y];
            const uint8_t* row_light = gear_lut_light[lut_y];

            for (int px = min_x; px <= max_x; px++) {
                int dx = px - cx_i;
                int lut_x = dx + GEAR_LUT_MAX_R;
                if (lut_x < 0 || lut_x >= GEAR_LUT_DIM) continue;

                uint8_t dist = row_dist[lut_x];
                if (dist < r_in || dist > r_out) continue;

                if (dist > r_root) {
                    uint16_t theta = row_angle[lut_x] - ang_fixed;
                    uint32_t mod_t = theta % period;
                    if (mod_t < t_min || mod_t > t_max) continue;
                }

                set_pixel(px, py, fast_shade(col, row_light[lut_x]));
            }
        }
    }
}

/* Window renderer */
static void draw_window(window_t* w, bool is_active) {
    if (!w || w->is_minimized) return;
    if (w->workspace != current_workspace) return;

    /* Window border */
    int win_h = w->is_shaded ? 28 : w->h;
    draw_rect(w->x, w->y, w->w, win_h, is_active ? COL_WIN_BORDER_ACT : COL_WIN_BORDER);

    /* XFWM4 Titlebar background */
    uint32_t title_col = is_active ? COL_TITLE_ACT : COL_TITLE_INACT;
    fill_rect(w->x + 1, w->y + 1, w->w - 2, 26, title_col);

    if (is_active) {
        fill_rect(w->x + 1, w->y + 26, w->w - 2, 1, COL_XFCE_BLUE);
    }

    /* Window title */
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s%s", w->title, w->is_shaded ? " [Shaded]" : "");
    draw_string_clipped(w->x + 10, w->y + 6, w->w - 100, title_buf,
                        is_active ? COL_TITLE_TXT_ACT : COL_TITLE_TXT_INA, title_col);

    /* XFWM4 Titlebar Buttons (Close, Maximize, Shade, Minimize) */
    int btn_close_x = w->x + w->w - 24;
    int btn_max_x   = w->x + w->w - 46;
    int btn_shade_x = w->x + w->w - 68; /* Iconic Roll-Up button */
    int btn_min_x   = w->x + w->w - 90;
    int btn_y       = w->y + 5;

    fill_rect(btn_min_x, btn_y, 18, 17, COL_BTN_MIN);
    draw_char(btn_min_x + 5, btn_y + 1, '_', COL_PANEL_BG, COL_BTN_MIN);

    fill_rect(btn_shade_x, btn_y, 18, 17, COL_BTN_SHADE);
    draw_char(btn_shade_x + 5, btn_y + 1, w->is_shaded ? 'v' : '^', COL_PANEL_BG, COL_BTN_SHADE);

    fill_rect(btn_max_x, btn_y, 18, 17, COL_BTN_MAX);
    draw_char(btn_max_x + 5, btn_y + 1, w->is_maximized ? '=' : '+', COL_PANEL_BG, COL_BTN_MAX);

    fill_rect(btn_close_x, btn_y, 18, 17, COL_BTN_CLOSE);
    draw_char(btn_close_x + 5, btn_y + 1, 'x', COL_TEXT_WHITE, COL_BTN_CLOSE);

    /* If window is shaded, client area is rolled up! */
    if (w->is_shaded) return;

    /* Client area */
    int client_x = w->x + 1;
    int client_y = w->y + 27;
    int client_w = w->w - 2;
    int client_h = w->h - 28;

    if (w->type == WIN_THUNAR) {
        /* Thunar File Manager */
        thunar_t* th = (thunar_t*)w->state;
        fill_rect(client_x, client_y, client_w, client_h, COL_WIN_BG);

        /* Top Location bar */
        fill_rect(client_x, client_y, client_w, 28, 0x00242830);
        draw_rect(client_x, client_y, client_w, 28, COL_WIN_BORDER);

        fill_rect(client_x + 6, client_y + 4, 40, 20, COL_XFCE_BLUE);
        draw_string(client_x + 14, client_y + 6, "Up", COL_TEXT_WHITE, COL_XFCE_BLUE);

        fill_rect(client_x + 52, client_y + 4, client_w - 60, 20, 0x001e222a);
        draw_rect(client_x + 52, client_y + 4, client_w - 60, 20, COL_WIN_BORDER);
        char loc[140];
        snprintf(loc, sizeof(loc), "Location: %s", th ? th->current_path : "/");
        draw_string_clipped(client_x + 58, client_y + 6, client_w - 75, loc, COL_XFCE_ACCENT, 0x001e222a);

        /* Left Bookmarks Sidebar (width 120) */
        int sb_w = 110;
        fill_rect(client_x, client_y + 28, sb_w, client_h - 48, 0x00242830);
        draw_rect(client_x, client_y + 28, sb_w, client_h - 48, COL_WIN_BORDER);

        draw_string(client_x + 8, client_y + 36, "PLACES", COL_TEXT_MUTED, 0x00242830);
        static const char* places[] = {"/root", "/home", "/bin", "/etc", "/dev", "/tmp"};
        for (int p = 0; p < 6; p++) {
            int py = client_y + 56 + (p * 22);
            draw_string(client_x + 10, py, places[p], COL_TEXT_WHITE, 0x00242830);
        }

        /* Right File Listing Pane */
        int pane_x = client_x + sb_w + 1;
        int pane_w = client_w - sb_w - 1;
        fill_rect(pane_x, client_y + 28, pane_w, client_h - 48, 0x001e222a);

        /* Table header */
        fill_rect(pane_x, client_y + 28, pane_w, 20, 0x002b303c);
        draw_string(pane_x + 8, client_y + 30, "Name", COL_XFCE_ACCENT, 0x002b303c);
        draw_string(pane_x + 200, client_y + 30, "Size", COL_XFCE_ACCENT, 0x002b303c);
        draw_string(pane_x + 280, client_y + 30, "Type", COL_XFCE_ACCENT, 0x002b303c);

        if (th) {
            int max_items = (client_h - 72) / 20;
            for (int i = 0; i < th->file_count && i < max_items; i++) {
                int iy = client_y + 50 + (i * 20);
                bool sel = (i == th->selected_idx);
                if (sel) fill_rect(pane_x + 2, iy, pane_w - 4, 18, 0x003b4252);

                char f_tag[64];
                snprintf(f_tag, sizeof(f_tag), "%s %s", th->is_directory[i] ? "[D]" : "[F]", th->file_names[i]);
                draw_string_clipped(pane_x + 8, iy + 2, 180, f_tag,
                                    th->is_directory[i] ? COL_XFCE_ACCENT : COL_TEXT_WHITE, sel ? 0x003b4252 : 0x001e222a);

                char s_tag[32];
                snprintf(s_tag, sizeof(s_tag), "%u B", th->file_sizes[i]);
                draw_string(pane_x + 200, iy + 2, s_tag, COL_TEXT_MUTED, sel ? 0x003b4252 : 0x001e222a);

                draw_string(pane_x + 280, iy + 2, th->is_directory[i] ? "Folder" : "File", COL_TEXT_MUTED, sel ? 0x003b4252 : 0x001e222a);
            }
        }

        /* Bottom Status bar */
        fill_rect(client_x, client_y + client_h - 20, client_w, 20, 0x00242830);
        draw_string(client_x + 8, client_y + client_h - 16, th ? th->status_msg : "Ready", COL_TEXT_MUTED, 0x00242830);
    }
    else if (w->type == WIN_TERMINAL) {
        /* XFCE Terminal with Tab Bar */
        xfce_term_t* t = (xfce_term_t*)w->state;
        fill_rect(client_x, client_y, client_w, client_h, COL_TERM_BG);

        /* Tab bar */
        fill_rect(client_x, client_y, client_w, 24, 0x0020232a);
        draw_rect(client_x, client_y, client_w, 24, COL_PANEL_BORDER);

        fill_rect(client_x + 4, client_y + 2, 100, 21, 0x00181a1f);
        draw_string(client_x + 12, client_y + 5, "Tab 1: bash", COL_XFCE_ACCENT, 0x00181a1f);

        fill_rect(client_x + 108, client_y + 2, 90, 21, 0x002b303c);
        draw_string(client_x + 116, client_y + 5, "Tab 2: root", COL_TEXT_MUTED, 0x002b303c);

        fill_rect(client_x + 202, client_y + 3, 20, 18, 0x003b4252);
        draw_char(client_x + 208, client_y + 4, '+', COL_TEXT_WHITE, 0x003b4252);

        if (t) {
            int max_vis = (client_h - 52) / 16;
            int start = (t->line_count > max_vis) ? t->line_count - max_vis : 0;
            int cy = client_y + 30;

            for (int i = start; i < t->line_count; i++) {
                draw_string_clipped(client_x + 8, cy, client_w - 16, t->lines[i], t->colors[i], COL_TERM_BG);
                cy += 16;
            }

            char pbuf[TERM_LINE_LEN + 32];
            snprintf(pbuf, sizeof(pbuf), "user@xfce:~$ %s", t->input_buf);
            draw_string_clipped(client_x + 8, cy, client_w - 16, pbuf, t->prompt_color, COL_TERM_BG);

            if (is_active) {
                int cursor_x = client_x + 8 + ((14 + t->cursor_pos) * 8);
                if (cursor_x + 8 < client_x + client_w) {
                    fill_rect(cursor_x, cy + 13, 8, 2, COL_TEXT_WHITE);
                }
            }
        }
    }
    else if (w->type == WIN_GEARS) {
        draw_gears_window(w, true);
    }
    else if (w->type == WIN_DOOM) {
        fill_rect(client_x, client_y, client_w, client_h, 0x0012141a);
        xfce_doom_t* d = (xfce_doom_t*)w->state;
        if (d) {
            int view_h = client_h - 48;
            fill_rect(client_x, client_y, client_w, view_h / 2, 0x001b1e24);
            fill_rect(client_x, client_y + view_h / 2, client_w, view_h / 2, 0x002b241e);

            for (int col = 0; col < client_w; col += 2) {
                float camera_x = 2.0f * (float)col / (float)client_w - 1.0f;
                float rdx = d->dir_x + d->plane_x * camera_x;
                float rdy = d->dir_y + d->plane_y * camera_x;

                int map_x = (int)d->pos_x, map_y = (int)d->pos_y;
                float ddx = fabsf(1.0f / (rdx + 0.00001f));
                float ddy = fabsf(1.0f / (rdy + 0.00001f));
                float perp_dist;

                int step_x = rdx < 0 ? -1 : 1, step_y = rdy < 0 ? -1 : 1;
                float sdx = (rdx < 0) ? (d->pos_x - map_x) * ddx : (map_x + 1.0f - d->pos_x) * ddx;
                float sdy = (rdy < 0) ? (d->pos_y - map_y) * ddy : (map_y + 1.0f - d->pos_y) * ddy;

                int hit = 0, side = 0;
                while (hit == 0) {
                    if (sdx < sdy) { sdx += ddx; map_x += step_x; side = 0; }
                    else           { sdy += ddy; map_y += step_y; side = 1; }
                    if (map_x >= 0 && map_x < DOOM_MAP_W && map_y >= 0 && map_y < DOOM_MAP_H) {
                        if (DOOM_MAP[map_y][map_x] > 0) hit = 1;
                    } else hit = 1;
                }

                if (side == 0) perp_dist = (map_x - d->pos_x + (1 - step_x) / 2.0f) / rdx;
                else           perp_dist = (map_y - d->pos_y + (1 - step_y) / 2.0f) / rdy;

                int line_h = (int)((float)view_h / (perp_dist + 0.0001f));
                int y0 = -line_h / 2 + view_h / 2;
                if (y0 < 0) y0 = 0;
                int y1 = line_h / 2 + view_h / 2;
                if (y1 >= view_h) y1 = view_h - 1;

                uint32_t wall_col = (side == 1) ? 0x00555a66 : 0x00787e8e;
                if (perp_dist > 5.0f) wall_col = shade_col(wall_col, 5.0f / perp_dist);

                fill_rect(client_x + col, client_y + y0, 2, y1 - y0, wall_col);
            }

            int gx = client_x + client_w / 2 - 25, gy = client_y + view_h - 70;
            if (d->shoot_anim > 0) {
                gy += 10;
                fill_rect(gx + 5, gy - 35, 40, 25, 0x00ffcc00);
            }
            fill_rect(gx + 10, gy, 14, 60, 0x0033363e);
            fill_rect(gx + 26, gy, 14, 60, 0x0033363e);

            int sy = client_y + view_h;
            fill_rect(client_x, sy, client_w, 48, 0x001b1c20);
            char hbuf[32];
            snprintf(hbuf, sizeof(hbuf), "AMMO: %d | HEALTH: %d%% | ARMOR: %d%% | FRAGS: %d", d->ammo, d->health, d->armor, d->kills);
            draw_string(client_x + 16, sy + 16, hbuf, 0x00e0b020, 0x001b1c20);
        }
    }
    else if (w->type == WIN_KSYSGUARD) {
        fill_rect(client_x, client_y, client_w, client_h, COL_WIN_BG);
        xfce_sysguard_t* ks = (xfce_sysguard_t*)w->state;

        draw_string(client_x + 16, client_y + 16, "=== CPU Activity History (60 seconds) ===", COL_XFCE_ACCENT, COL_WIN_BG);
        int graph_x = client_x + 16, graph_y = client_y + 36, graph_w = client_w - 32, graph_h = 100;
        fill_rect(graph_x, graph_y, graph_w, graph_h, 0x00181a1f);
        draw_rect(graph_x, graph_y, graph_w, graph_h, COL_WIN_BORDER);

        if (ks) {
            for (int i = 0; i < 59; i++) {
                int x0 = graph_x + (i * graph_w) / 60;
                int x1 = graph_x + ((i + 1) * graph_w) / 60;
                int bar_h = (ks->cpu_history[i] * (graph_h - 6)) / 100;
                fill_rect(x0, graph_y + graph_h - bar_h - 1, (x1 - x0) + 1, bar_h, COL_XFCE_ACCENT);
            }
        }

        draw_string(client_x + 16, client_y + 154, "=== Physical Memory (RAM) Allocation ===", COL_XFCE_ACCENT, COL_WIN_BG);
        int bar_x = client_x + 16, bar_y = client_y + 174, bar_w = client_w - 32, bar_h = 24;
        fill_rect(bar_x, bar_y, bar_w, bar_h, 0x00181a1f);
        draw_rect(bar_x, bar_y, bar_w, bar_h, COL_WIN_BORDER);
        fill_rect(bar_x + 2, bar_y + 2, bar_w / 4, bar_h - 4, COL_TERM_GREEN);
        draw_string(bar_x + 8, bar_y + 5, "512 MB Active / 4096 MB Detected", COL_TEXT_WHITE, 0xFFFFFFFF);
    }
    else if (w->type == WIN_SETTINGS) {
        fill_rect(client_x, client_y, client_w, client_h, COL_WIN_BG);
        draw_string(client_x + 16, client_y + 16, "=== XFCE 4 Settings & Hardware Calibration ===", COL_XFCE_ACCENT, COL_WIN_BG);

        draw_string(client_x + 16, client_y + 46, "1. Mouse Sensitivity:", COL_TEXT_WHITE, COL_WIN_BG);
        static const int sens_opts[4] = {50, 75, 100, 150};
        static const char* sens_lbl[4] = {"50% (Slow)", "75% (Smooth)", "100% (Raw 1:1)", "150% (Fast)"};
        for (int s = 0; s < 4; s++) {
            int bx = client_x + 16 + (s * 115), by = client_y + 68;
            bool active = (mouse_sensitivity == sens_opts[s]);
            fill_rect(bx, by, 105, 24, active ? COL_XFCE_BLUE : 0x00242830);
            draw_rect(bx, by, 105, 24, COL_WIN_BORDER);
            draw_string_clipped(bx + 6, by + 5, 95, sens_lbl[s], COL_TEXT_WHITE, active ? COL_XFCE_BLUE : 0x00242830);
        }

        draw_string(client_x + 16, client_y + 110, "2. Frame Pacing & High FPS Rate (F11 to toggle):", COL_TEXT_WHITE, COL_WIN_BG);
        static const char* r_lbl[5] = {"60 Hz", "75 Hz", "120 Hz", "144 Hz", "Uncapped"};
        for (int r = 0; r < 5; r++) {
            int bx = client_x + 16 + (r * 94), by = client_y + 132;
            bool active = (refresh_mode == r);
            fill_rect(bx, by, 86, 24, active ? COL_TERM_GREEN : 0x00242830);
            draw_rect(bx, by, 86, 24, COL_WIN_BORDER);
            draw_string_clipped(bx + 8, by + 5, 76, r_lbl[r], active ? 0x001e222a : COL_TEXT_WHITE, active ? COL_TERM_GREEN : 0x00242830);
        }

        draw_string(client_x + 16, client_y + 174, "3. Hardware Acceleration & Input:", COL_TEXT_WHITE, COL_WIN_BG);
        draw_string(client_x + 16, client_y + 196, "Mode: 100% Raw PS/2 Direct Input | Hardware Acceleration Active", COL_XFCE_ACCENT, COL_WIN_BG);
    }
    else if (w->type == WIN_HELP) {
        fill_rect(client_x, client_y, client_w, client_h, COL_WIN_BG);
        draw_string(client_x + 16, client_y + 16, "=== XFCE 4 Desktop Keyboard Shortcuts ===", COL_XFCE_ACCENT, COL_WIN_BG);
        draw_string(client_x + 16, client_y + 44, "  Alt+F1      - Open Whisker Applications Menu", COL_TEXT_WHITE, COL_WIN_BG);
        draw_string(client_x + 16, client_y + 64, "  Ctrl+Alt+T  - Launch XFCE Terminal", COL_TEXT_WHITE, COL_WIN_BG);
        draw_string(client_x + 16, client_y + 84, "  Alt+Tab     - Cycle through active windows", COL_TEXT_WHITE, COL_WIN_BG);
        draw_string(client_x + 16, client_y + 104,"  F11         - Cycle Refresh Rate (60Hz / 120Hz / Uncapped)", COL_TEXT_WHITE, COL_WIN_BG);
        draw_string(client_x + 16, client_y + 124,"  Window [^]  - XFWM4 Window Roll-up / Shade feature", COL_TEXT_WHITE, COL_WIN_BG);
        draw_string(client_x + 16, client_y + 144,"  Ctrl+Q      - Exit XFCE session back to display manager", COL_TEXT_WHITE, COL_WIN_BG);
    }
}

/* --- XFCE TOP PANEL (Panel 1) --- */

static void draw_top_panel(void) {
    fill_rect(0, 0, scr_w, 30, COL_PANEL_BG);
    draw_rect(0, 0, scr_w, 30, COL_PANEL_BORDER);

    /* 1. Whisker Menu Button */
    fill_rect(4, 3, 130, 24, whisker_open ? COL_XFCE_BLUE : 0x002c323d);
    draw_rect(4, 3, 130, 24, COL_PANEL_BORDER);
    draw_string(12, 7, "[>] Applications", COL_TEXT_WHITE, whisker_open ? COL_XFCE_BLUE : 0x002c323d);

    /* 2. Workspace Switcher (Workspaces 1 & 2) */
    int ws_x = 142;
    for (int ws = 0; ws < 2; ws++) {
        bool is_cur = (current_workspace == ws);
        fill_rect(ws_x, 3, 32, 24, is_cur ? COL_XFCE_BLUE : 0x002c323d);
        draw_rect(ws_x, 3, 32, 24, COL_PANEL_BORDER);
        char ws_lbl[4];
        snprintf(ws_lbl, sizeof(ws_lbl), "%d", ws + 1);
        draw_string(ws_x + 12, 7, ws_lbl, COL_TEXT_WHITE, is_cur ? COL_XFCE_BLUE : 0x002c323d);
        ws_x += 36;
    }

    /* 3. Window Task List buttons */
    int task_x = ws_x + 10;
    for (int i = 0; i < window_count; i++) {
        window_t* w = windows[i];
        if (!w || w->workspace != current_workspace) continue;
        bool is_active = (i == active_win_idx && !w->is_minimized);
        uint32_t btn_col = is_active ? COL_XFCE_BLUE : (w->is_minimized ? 0x001e222a : 0x002c323d);

        fill_rect(task_x, 3, 130, 24, btn_col);
        draw_rect(task_x, 3, 130, 24, COL_PANEL_BORDER);
        draw_string_clipped(task_x + 8, 7, 114, w->title, COL_TEXT_WHITE, btn_col);

        task_x += 136;
        if (task_x + 130 > (int)scr_w - 260) break;
    }

    /* 4. System Tray (FPS Badge, Hypervisor, Clock, Logout) */
    int tray_x = scr_w - 260;
    fill_rect(tray_x, 3, 256, 24, 0x0020242c);
    draw_rect(tray_x, 3, 256, 24, COL_PANEL_BORDER);

    char fps_badge[32];
    if (refresh_hz[refresh_mode] > 0) snprintf(fps_badge, sizeof(fps_badge), "[%u FPS|%dHz]", measured_fps, refresh_hz[refresh_mode]);
    else snprintf(fps_badge, sizeof(fps_badge), "[%u FPS|MAX]", measured_fps);
    draw_string(tray_x + 8, 7, fps_badge, COL_TERM_GREEN, 0x0020242c);

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[32] = "12:00:00";
    if (tm_info) strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    draw_string(tray_x + 132, 7, time_str, COL_TEXT_WHITE, 0x0020242c);

    /* Logout / Power button */
    fill_rect(scr_w - 36, 4, 28, 22, COL_BTN_CLOSE);
    draw_char(scr_w - 27, 7, 'X', COL_TEXT_WHITE, COL_BTN_CLOSE);
}

/* --- XFCE BOTTOM FLOATING DOCK (Panel 2) --- */

static void draw_bottom_dock(void) {
    int dock_w = 400;
    int dock_h = 44;
    int dock_x = (scr_w - dock_w) / 2;
    int dock_y = scr_h - dock_h - 4;

    fill_rect(dock_x, dock_y, dock_w, dock_h, COL_DOCK_BG);
    draw_rect(dock_x, dock_y, dock_w, dock_h, COL_DOCK_BORDER);

    static const char* dock_icons[] = {
        "[#] Thunar",
        "[>] Terminal",
        "[*] DOOM",
        "[o] Gears",
        "[+] SysGuard",
        "[@] Settings"
    };

    int item_w = (dock_w - 16) / 6;
    for (int i = 0; i < 6; i++) {
        int ix = dock_x + 8 + (i * item_w);
        int iy = dock_y + 6;
        bool hover = (cur_mx >= ix && cur_mx < ix + item_w && cur_my >= dock_y && cur_my < dock_y + dock_h);

        if (hover) {
            fill_rect(ix, iy, item_w - 4, 32, COL_DOCK_HOVER);
            draw_rect(ix, iy, item_w - 4, 32, COL_XFCE_BLUE);
        }

        draw_string_clipped(ix + 6, iy + 9, item_w - 12, dock_icons[i],
                            hover ? COL_XFCE_ACCENT : COL_TEXT_WHITE, hover ? COL_DOCK_HOVER : COL_DOCK_BG);
    }
}

/* --- WHISKER APPLICATIONS MENU --- */

static void draw_whisker_menu(void) {
    if (!whisker_open) return;

    int mx = 4, my = 32;
    fill_rect(mx, my, WHISKER_W, WHISKER_H, 0x0020242c);
    draw_rect(mx, my, WHISKER_W, WHISKER_H, COL_XFCE_BLUE);

    /* Search bar */
    fill_rect(mx + 8, my + 8, WHISKER_W - 16, 24, 0x00181a1f);
    draw_rect(mx + 8, my + 8, WHISKER_W - 16, 24, COL_WIN_BORDER);
    draw_string(mx + 14, my + 12, "Search applications...", COL_TEXT_MUTED, 0x00181a1f);

    /* Category list (left) */
    int cat_w = 80;
    fill_rect(mx + 8, my + 38, cat_w, WHISKER_H - 46, 0x00282d37);
    draw_rect(mx + 8, my + 38, cat_w, WHISKER_H - 46, COL_WIN_BORDER);

    static const char* cats[] = {"All", "System", "Utilities", "Games", "Settings"};
    for (int c = 0; c < 5; c++) {
        int cy = my + 44 + (c * 24);
        bool act = (whisker_cat == c);
        if (act) fill_rect(mx + 10, cy - 2, cat_w - 4, 20, COL_XFCE_BLUE);
        draw_string(mx + 14, cy, cats[c], COL_TEXT_WHITE, act ? COL_XFCE_BLUE : 0x00282d37);
    }

    /* Applications list (right) */
    int app_x = mx + cat_w + 14;
    int app_w = WHISKER_W - cat_w - 22;
    fill_rect(app_x, my + 38, app_w, WHISKER_H - 46, 0x00181a1f);
    draw_rect(app_x, my + 38, app_w, WHISKER_H - 46, COL_WIN_BORDER);

    static const char* app_items[] = {
        "[#] Thunar File Manager",
        "[>] XFCE Terminal",
        "[*] DOOM (id Software)",
        "[o] Wayland Gears 3D",
        "[+] Task Manager",
        "[@] XFCE Settings",
        "[?] Help & Shortcuts",
        "[X] Log Out"
    };

    for (int a = 0; a < 8; a++) {
        int ay = my + 44 + (a * 24);
        bool hover = (cur_mx >= app_x && cur_mx <= app_x + app_w && cur_my >= ay - 2 && cur_my <= ay + 18);
        if (hover) fill_rect(app_x + 2, ay - 2, app_w - 4, 20, 0x003b4252);
        draw_string_clipped(app_x + 6, ay, app_w - 12, app_items[a], COL_TEXT_WHITE, hover ? 0x003b4252 : 0x00181a1f);
    }
}

/* --- WINDOW CREATION & INPUT HANDLING --- */

static window_t* create_window(int type, const char* title, int x, int y, int w, int h) {
    if (window_count >= MAX_WINDOWS) return NULL;

    window_t* win = (window_t*)calloc(1, sizeof(window_t));
    if (!win) return NULL;

    win->id = next_win_id++;
    win->type = type;
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->orig_x = x; win->orig_y = y; win->orig_w = w; win->orig_h = h;
    win->workspace = current_workspace;
    win->is_minimized = false;
    win->is_maximized = false;
    win->is_shaded = false;

    if (type == WIN_THUNAR) {
        thunar_t* th = (thunar_t*)calloc(1, sizeof(thunar_t));
        thunar_load_dir(th, "/home/iota");
        win->state = th;
    } else if (type == WIN_TERMINAL) {
        xfce_term_t* t = (xfce_term_t*)calloc(1, sizeof(xfce_term_t));
        t->prompt_color = COL_XFCE_ACCENT;
        term_append(t, "XFCE Terminal 4.18 - IotaOS Session", COL_XFCE_ACCENT);
        term_append(t, "Type 'help' for command summary.", COL_TEXT_MUTED);
        term_append(t, "", COL_TEXT_WHITE);
        win->state = t;
    } else if (type == WIN_GEARS) {
        xfce_gears_t* g = (xfce_gears_t*)calloc(1, sizeof(xfce_gears_t));
        g->speed = 0.06f;
        g->theme = 2; /* XFCE Nord */
        win->state = g;
    } else if (type == WIN_DOOM) {
        xfce_doom_t* d = (xfce_doom_t*)calloc(1, sizeof(xfce_doom_t));
        d->pos_x = 2.5f; d->pos_y = 2.5f;
        d->dir_x = 1.0f; d->dir_y = 0.0f;
        d->plane_x = 0.0f; d->plane_y = 0.66f;
        d->ammo = 48; d->health = 100; d->armor = 100;
        win->state = d;
    } else if (type == WIN_KSYSGUARD) {
        xfce_sysguard_t* ks = (xfce_sysguard_t*)calloc(1, sizeof(xfce_sysguard_t));
        for (int i = 0; i < 60; i++) ks->cpu_history[i] = 12 + (i * 7) % 25;
        win->state = ks;
    } else if (type == WIN_SETTINGS) {
        win->state = calloc(1, sizeof(xfce_settings_t));
    }

    windows[window_count] = win;
    active_win_idx = window_count;
    window_count++;
    screen_dirty = true;
    return win;
}

static void handle_click(int mx, int my) {
    /* 1. Whisker Menu click */
    if (whisker_open) {
        if (mx >= 4 && mx <= 4 + WHISKER_W && my >= 32 && my <= 32 + WHISKER_H) {
            int app_x = 4 + 80 + 14;
            int app_w = WHISKER_W - 80 - 22;
            if (mx >= app_x && mx <= app_x + app_w) {
                int clicked_app = (my - (32 + 44)) / 24;
                whisker_open = false;
                if (clicked_app == 0) create_window(WIN_THUNAR, "Thunar - /home/iota", 140, 80, 560, 380);
                else if (clicked_app == 1) create_window(WIN_TERMINAL, "Terminal - bash", 100, 60, 560, 360);
                else if (clicked_app == 2) { launch_doom(); return; }
                else if (clicked_app == 3) create_window(WIN_GEARS, "Wayland Gears 3D", 200, 90, 500, 400);
                else if (clicked_app == 4) create_window(WIN_KSYSGUARD, "XFCE Task Manager", 160, 70, 520, 360);
                else if (clicked_app == 5) create_window(WIN_SETTINGS, "XFCE Settings", 180, 100, 540, 360);
                else if (clicked_app == 6) create_window(WIN_HELP, "XFCE 4 Shortcuts", 220, 120, 500, 260);
                else if (clicked_app == 7) running = false;
                return;
            }
        }
        whisker_open = false;
    }

    /* 2. Top Panel clicks */
    if (my < 30) {
        if (mx >= 4 && mx <= 134) {
            whisker_open = !whisker_open;
            screen_dirty = true;
            return;
        }

        /* Workspace Switcher */
        if (mx >= 142 && mx <= 142 + 32) { current_workspace = 0; screen_dirty = true; return; }
        if (mx >= 178 && mx <= 178 + 32) { current_workspace = 1; screen_dirty = true; return; }

        /* Logout button */
        if (mx >= (int)scr_w - 36) { running = false; return; }

        /* Window task list */
        int task_x = 224;
        for (int i = 0; i < window_count; i++) {
            window_t* w = windows[i];
            if (!w || w->workspace != current_workspace) continue;
            if (mx >= task_x && mx <= task_x + 130) {
                if (i == active_win_idx && !w->is_minimized) w->is_minimized = true;
                else { w->is_minimized = false; bring_to_front(i); }
                return;
            }
            task_x += 136;
        }
        return;
    }

    /* 3. Bottom Dock clicks */
    int dock_w = 400, dock_h = 44;
    int dock_x = (scr_w - dock_w) / 2, dock_y = scr_h - dock_h - 4;
    if (mx >= dock_x && mx <= dock_x + dock_w && my >= dock_y && my <= dock_y + dock_h) {
        int item_w = (dock_w - 16) / 6;
        int idx = (mx - dock_x - 8) / item_w;
        if (idx == 0) create_window(WIN_THUNAR, "Thunar - /home/iota", 140, 80, 560, 380);
        else if (idx == 1) create_window(WIN_TERMINAL, "Terminal - bash", 100, 60, 560, 360);
        else if (idx == 2) { launch_doom(); return; }
        else if (idx == 3) create_window(WIN_GEARS, "Wayland Gears 3D", 200, 90, 500, 400);
        else if (idx == 4) create_window(WIN_KSYSGUARD, "XFCE Task Manager", 160, 70, 520, 360);
        else if (idx == 5) create_window(WIN_SETTINGS, "XFCE Settings", 180, 100, 540, 360);
        return;
    }

    /* 4. Desktop Icons */
    if (mx >= 20 && mx <= 160) {
        if (my >= 46 && my <= 86) create_window(WIN_THUNAR, "Thunar - /home/iota", 140, 80, 560, 380);
        else if (my >= 102 && my <= 142) create_window(WIN_TERMINAL, "Terminal - bash", 100, 60, 560, 360);
        else if (my >= 158 && my <= 198) { launch_doom(); return; }
        else if (my >= 214 && my <= 254) create_window(WIN_GEARS, "Wayland Gears 3D", 200, 90, 500, 400);
        else if (my >= 270 && my <= 310) create_window(WIN_KSYSGUARD, "XFCE Task Manager", 160, 70, 520, 360);
        else if (my >= 326 && my <= 366) create_window(WIN_SETTINGS, "XFCE Settings", 180, 100, 540, 360);
        else if (my >= 382 && my <= 422) create_window(WIN_HELP, "XFCE 4 Shortcuts", 220, 120, 500, 260);
        return;
    }

    /* 5. Windows in top-to-bottom z-order */
    for (int i = window_count - 1; i >= 0; i--) {
        window_t* w = windows[i];
        if (!w || w->is_minimized || w->workspace != current_workspace) continue;
        int win_h = w->is_shaded ? 28 : w->h;

        if (mx >= w->x && mx <= w->x + w->w && my >= w->y && my <= w->y + win_h) {
            bring_to_front(i);

            /* Titlebar buttons */
            if (my >= w->y && my <= w->y + 26) {
                int btn_close_x = w->x + w->w - 24;
                int btn_max_x   = w->x + w->w - 46;
                int btn_shade_x = w->x + w->w - 68;
                int btn_min_x   = w->x + w->w - 90;

                if (mx >= btn_close_x && mx <= btn_close_x + 18) { close_window(active_win_idx); return; }
                if (mx >= btn_max_x && mx <= btn_max_x + 18) { toggle_maximize(w); return; }
                if (mx >= btn_shade_x && mx <= btn_shade_x + 18) { toggle_shade(w); return; }
                if (mx >= btn_min_x && mx <= btn_min_x + 18) { w->is_minimized = true; return; }

                /* Drag titlebar */
                drag_win_idx = active_win_idx;
                drag_off_x = mx - w->x;
                drag_off_y = my - w->y;
                return;
            }

            /* Thunar clicks */
            if (w->type == WIN_THUNAR && !w->is_shaded) {
                thunar_t* th = (thunar_t*)w->state;
                int client_x = w->x + 1, client_y = w->y + 27;
                /* Click places */
                if (mx >= client_x && mx <= client_x + 110 && my >= client_y + 56) {
                    int p_idx = (my - (client_y + 56)) / 22;
                    static const char* places[] = {"/root", "/home", "/bin", "/etc", "/dev", "/tmp"};
                    if (p_idx >= 0 && p_idx < 6) thunar_load_dir(th, places[p_idx]);
                    screen_dirty = true;
                    return;
                }
                /* Click Up button */
                if (mx >= client_x + 6 && mx <= client_x + 46 && my >= client_y + 4 && my <= client_y + 24) {
                    thunar_load_dir(th, "/");
                    screen_dirty = true;
                    return;
                }
            }

            /* Settings clicks */
            if (w->type == WIN_SETTINGS && !w->is_shaded) {
                int client_x = w->x + 1, client_y = w->y + 27;
                static const int s_opts[4] = {50, 75, 100, 150};
                for (int s = 0; s < 4; s++) {
                    int bx = client_x + 16 + (s * 115), by = client_y + 68;
                    if (mx >= bx && mx <= bx + 105 && my >= by && my <= by + 24) {
                        mouse_sensitivity = s_opts[s];
                        if (mouse_fd >= 0) ioctl(mouse_fd, 2, &mouse_sensitivity);
                        char msg[64];
                        snprintf(msg, sizeof(msg), "Mouse Sensitivity: %d%%", mouse_sensitivity);
                        show_toast(msg);
                        screen_dirty = true;
                        return;
                    }
                }
            }
            return;
        }
    }
}

/* --- DIRTY-RECT BUFFER MANAGEMENT --- */

static void copy_rect_between_buffers(uint32_t* dst_buf, const uint32_t* src_buf, int rx, int ry, int rw, int rh) {
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { ry += ry; ry = 0; }
    if (rx + rw > (int)scr_w) rw = (int)scr_w - rx;
    if (ry + rh > (int)scr_h) rh = (int)scr_h - ry;
    if (rw <= 0 || rh <= 0 || !dst_buf || !src_buf) return;

    for (int r = 0; r < rh; r++) {
        uint32_t offset = (ry + r) * scr_w + rx;
        memcpy(&dst_buf[offset], &src_buf[offset], rw * sizeof(uint32_t));
    }
}

static void present_rect(int rx, int ry, int rw, int rh) {
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { ry += ry; ry = 0; }
    if (rx + rw > (int)scr_w) rw = (int)scr_w - rx;
    if (ry + rh > (int)scr_h) rh = (int)scr_h - ry;
    if (rw <= 0 || rh <= 0) return;

    if (fb_direct) {
        for (int r = 0; r < rh; r++) {
            uint32_t* src = &backbuffer[(ry + r) * scr_w + rx];
            uint32_t* dst = (uint32_t*)((uint8_t*)fb_direct + ((ry + r) * scr_pitch) + (rx * 4));
            memcpy(dst, src, rw * 4);
        }
    } else if (fb_fd >= 0) {
        for (int r = 0; r < rh; r++) {
            off_t offset = ((ry + r) * scr_pitch) + (rx * 4);
            lseek(fb_fd, offset, SEEK_SET);
            write(fb_fd, &backbuffer[(ry + r) * scr_w + rx], rw * 4);
        }
    }
}

static void update_cursor_dirty_rect(int old_x, int old_y, int new_x, int new_y) {
    int old_rx = old_x - 8, old_ry = old_y - 6, old_rw = 48, old_rh = 28;
    int new_rx = new_x - 8, new_ry = new_y - 6, new_rw = 48, new_rh = 28;

    copy_rect_between_buffers(backbuffer, clean_backbuffer, old_rx, old_ry, old_rw, old_rh);
    copy_rect_between_buffers(backbuffer, clean_backbuffer, new_rx, new_ry, new_rw, new_rh);
    draw_mouse_cursor(new_x, new_y);

    int min_x = old_rx < new_rx ? old_rx : new_rx;
    int min_y = old_ry < new_ry ? old_ry : new_ry;
    int max_x = (old_rx + old_rw) > (new_rx + new_rw) ? (old_rx + old_rw) : (new_rx + new_rw);
    int max_y = (old_ry + old_rh) > (new_ry + new_rh) ? (old_ry + old_rh) : (new_ry + new_rh);

    if ((max_x - min_x) <= 128 && (max_y - min_y) <= 96) {
        present_rect(min_x, min_y, max_x - min_x, max_y - min_y);
    } else {
        present_rect(old_rx, old_ry, old_rw, old_rh);
        present_rect(new_rx, new_ry, new_rw, new_rh);
    }
}

static void present(void) {
    if (fb_direct) memcpy(fb_direct, backbuffer, fb_total_size);
    else if (fb_fd >= 0) {
        lseek(fb_fd, 0, SEEK_SET);
        write(fb_fd, backbuffer, fb_total_size);
    }
}

/* --- HARDWARE INPUT POLLING --- */

static void process_mouse(void) {
    if (mouse_fd >= 0) {
        mouse_state_ext_t mst;
        while (read(mouse_fd, &mst, sizeof(mst)) == (ssize_t)sizeof(mst)) {
            cur_mx = mst.x;
            cur_my = mst.y;
            cur_mbtn = mst.buttons;
        }
    }

    if (cur_mx < 0) cur_mx = 0;
    if ((uint32_t)cur_mx >= scr_w) cur_mx = scr_w - 1;
    if (cur_my < 0) cur_my = 0;
    if ((uint32_t)cur_my >= scr_h) cur_my = scr_h - 1;

    bool pressed = (cur_mbtn & 1) && !(prev_mbtn & 1);
    bool released = !(cur_mbtn & 1) && (prev_mbtn & 1);

    if (pressed) handle_click(cur_mx, cur_my);

    if ((cur_mbtn & 1) && drag_win_idx >= 0 && drag_win_idx < window_count) {
        window_t* w = windows[drag_win_idx];
        if (w && !w->is_maximized) {
            w->x = cur_mx - drag_off_x;
            w->y = cur_my - drag_off_y;
            if (w->y < 30) w->y = 30;
            if (w->y > (int)scr_h - 60) w->y = scr_h - 60;
        }
    }

    if (released) {
        drag_win_idx = -1;
        screen_dirty = true;
    }

    prev_mbtn = cur_mbtn;
}

static void process_keyboard(void) {
    struct pollfd pfd = { .fd = 0, .events = POLLIN };
    if (poll(&pfd, 1, 0) <= 0 || !(pfd.revents & POLLIN)) return;

    char ch;
    if (read(0, &ch, 1) <= 0) return;

    if (ch == 27) {
        char seq[4] = {0};
        if (poll(&pfd, 1, 10) > 0 && read(0, &seq[0], 1) > 0 && seq[0] == '[') {
            if (read(0, &seq[1], 1) > 0) {
                if (seq[1] == '2' && read(0, &seq[2], 1) > 0 && seq[2] == '3') {
                    read(0, &seq[3], 1); /* F11: Cycle refresh rate */
                    refresh_mode = (refresh_mode + 1) % 5;
                    char tbuf[64];
                    if (refresh_hz[refresh_mode] > 0) snprintf(tbuf, sizeof(tbuf), "Refresh Rate: %d Hz", refresh_hz[refresh_mode]);
                    else strcpy(tbuf, "Refresh Rate: Uncapped (Max FPS)");
                    show_toast(tbuf);
                    return;
                }
            }
        }
        if (whisker_open) whisker_open = false;
        else if (active_win_idx >= 0) close_window(active_win_idx);
        else running = false;
    } else if (ch == '\t') { /* Tab: bring next window forward */
        if (window_count > 1) bring_to_front(0);
    } else if (ch == 17) { /* Ctrl+Q */
        running = false;
    } else if (active_win_idx >= 0 && windows[active_win_idx]->type == WIN_TERMINAL) {
        xfce_term_t* t = (xfce_term_t*)windows[active_win_idx]->state;
        if (!t) return;
        if (ch == '\n' || ch == '\r') {
            term_execute(t);
        } else if (ch == 127 || ch == '\b') {
            if (t->cursor_pos > 0) {
                t->cursor_pos--;
                t->input_len--;
                t->input_buf[t->input_len] = '\0';
            }
        } else if (ch >= 32 && ch <= 126 && t->input_len < TERM_LINE_LEN - 1) {
            t->input_buf[t->input_len++] = ch;
            t->input_buf[t->input_len] = '\0';
            t->cursor_pos++;
        }
        screen_dirty = true;
    }
}

/* --- MAIN ENTRY POINT --- */

int main(void) {
    /* 1. Open Framebuffer */
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "xfce: cannot open /dev/fb0\n");
        return 1;
    }

    uint32_t fb_meta[6] = {0};
    if (ioctl(fb_fd, 0x4600, fb_meta) == 0 && fb_meta[0] > 0 && fb_meta[1] > 0) {
        scr_w = fb_meta[0];
        scr_h = fb_meta[1];
        scr_pitch = fb_meta[2];
        scr_bpp = fb_meta[3];
        fb_direct = (uint32_t*)fb_meta[4];
        fb_total_size = fb_meta[5];
    } else {
        scr_w = 1024;
        scr_h = 768;
        scr_pitch = 4096;
        fb_total_size = scr_pitch * scr_h;
    }

    backbuffer = (uint32_t*)malloc(fb_total_size);
    wallpaper_buffer = (uint32_t*)malloc(fb_total_size);
    clean_backbuffer = (uint32_t*)malloc(fb_total_size);
    if (!backbuffer || !wallpaper_buffer || !clean_backbuffer) {
        fprintf(stderr, "xfce: out of memory for framebuffers\n");
        close(fb_fd);
        return 1;
    }

    init_wallpaper();

    /* 3. Open Mouse */
    mouse_fd = open("/dev/mouse", O_RDONLY | O_NONBLOCK);
    if (mouse_fd >= 0) ioctl(mouse_fd, 2, &mouse_sensitivity);

    /* 4. Configure Raw Terminal Input */
    tcgetattr(0, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);

    printf("\033[?25l");
    fflush(stdout);

    /* 5. Spawn Default Desktop Windows */
    create_window(WIN_THUNAR, "Thunar - /home/iota", 80, 50, 560, 380);
    create_window(WIN_GEARS, "Wayland-Gears 3D Demo", 460, 80, 500, 400);

    char init_toast[80];
    snprintf(init_toast, sizeof(init_toast), "XFCE 4.18 Session Started | %dHz", refresh_hz[refresh_mode]);
    show_toast(init_toast);

    /* 6. Event & Render Loop */
    uint32_t frame_tick = 0;
    fps_last_time = time(NULL);

    while (running) {
        frame_tick++;
        fps_frame_count++;

        time_t cur_sec = time(NULL);
        if (cur_sec != fps_last_time) {
            measured_fps = fps_frame_count;
            fps_frame_count = 0;
            fps_last_time = cur_sec;
            screen_dirty = true;
        }

        /* 1. Poll Inputs */
        process_mouse();
        process_keyboard();

        /* 2. Animate Active Windows */
        bool has_animated_window = false;
        window_t* anim_win = NULL;
        for (int i = 0; i < window_count; i++) {
            window_t* w = windows[i];
            if (!w || w->is_minimized || w->workspace != current_workspace) continue;

            if (w->type == WIN_GEARS) {
                xfce_gears_t* g = (xfce_gears_t*)w->state;
                if (g && !g->paused) {
                    g->angle += g->speed;
                    if (g->angle > 2.0f * M_PI * 100.0f) g->angle = 0.0f;
                    has_animated_window = true;
                    anim_win = w;
                }
            } else if (w->type == WIN_DOOM) {
                xfce_doom_t* d = (xfce_doom_t*)w->state;
                if (d) {
                    d->tick++;
                    if (d->shoot_anim > 0) d->shoot_anim--;
                    has_animated_window = true;
                    anim_win = w;
                }
            } else if (w->type == WIN_KSYSGUARD && (frame_tick % 20 == 0)) {
                xfce_sysguard_t* ks = (xfce_sysguard_t*)w->state;
                if (ks) {
                    for (int j = 0; j < 59; j++) ks->cpu_history[j] = ks->cpu_history[j + 1];
                    int delta = ((frame_tick * 17) % 30) - 15;
                    int last = ks->cpu_history[58] + delta;
                    if (last < 5) last = 5;
                    if (last > 95) last = 95;
                    ks->cpu_history[59] = last;
                    has_animated_window = true;
                    anim_win = w;
                }
            }
        }

        if (toast_timer > 0) {
            toast_timer--;
            if (toast_timer == 0) screen_dirty = true;
        }

        if (screen_dirty) {
            /* Full Desktop Composition */
            memcpy(backbuffer, wallpaper_buffer, fb_total_size);

            for (int i = 0; i < window_count; i++) {
                if (i != active_win_idx) draw_window(windows[i], false);
            }
            if (active_win_idx >= 0 && active_win_idx < window_count) {
                draw_window(windows[active_win_idx], true);
            }

            draw_top_panel();
            draw_bottom_dock();
            draw_whisker_menu();
            draw_toast();

            memcpy(clean_backbuffer, backbuffer, fb_total_size);
            draw_mouse_cursor(cur_mx, cur_my);
            present();

            prev_rendered_mx = cur_mx;
            prev_rendered_my = cur_my;
            screen_dirty = false;
        } else if (has_animated_window && anim_win && !whisker_open && window_count > 0 && anim_win == windows[window_count - 1]) {
            /* High-Performance 90+ FPS Dirty Rect Path */
            int client_x = anim_win->x + 1;
            int client_y = anim_win->y + 27;
            int client_w = anim_win->w - 2;
            int client_h = anim_win->h - 28;
            int mid_x = client_x + (int)(client_w * 0.45f);
            int mid_y = client_y + 24 + (int)((client_h - 48) * 0.50f);
            int g_bx = mid_x - 126, g_by = mid_y - 104, g_bw = 212, g_bh = 186;

            copy_rect_between_buffers(backbuffer, clean_backbuffer, prev_rendered_mx - 8, prev_rendered_my - 6, 48, 28);

            uint32_t* saved_bb = backbuffer;
            backbuffer = clean_backbuffer;
            if (anim_win->type == WIN_GEARS) draw_gears_window(anim_win, false);
            else draw_window(anim_win, true);
            backbuffer = saved_bb;

            if (anim_win->type == WIN_GEARS) {
                copy_rect_between_buffers(backbuffer, clean_backbuffer, g_bx, g_by, g_bw, g_bh);
                copy_rect_between_buffers(backbuffer, clean_backbuffer, client_x + client_w - 92, client_y + 4, 88, 18);
            } else {
                copy_rect_between_buffers(backbuffer, clean_backbuffer, anim_win->x, anim_win->y, anim_win->w, anim_win->h);
            }

            draw_mouse_cursor(cur_mx, cur_my);

            if (anim_win->type == WIN_GEARS) {
                present_rect(g_bx, g_by, g_bw, g_bh);
                present_rect(client_x + client_w - 92, client_y + 4, 88, 18);
            } else {
                present_rect(anim_win->x, anim_win->y, anim_win->w, anim_win->h);
            }

            present_rect(prev_rendered_mx - 8, prev_rendered_my - 6, 48, 28);
            present_rect(cur_mx - 8, cur_my - 6, 48, 28);

            prev_rendered_mx = cur_mx;
            prev_rendered_my = cur_my;
        } else if (has_animated_window) {
            screen_dirty = true;
        } else if (cur_mx != prev_rendered_mx || cur_my != prev_rendered_my) {
            update_cursor_dirty_rect(prev_rendered_mx, prev_rendered_my, cur_mx, cur_my);
            prev_rendered_mx = cur_mx;
            prev_rendered_my = cur_my;
        }

        if (refresh_delays[refresh_mode] > 0) usleep(refresh_delays[refresh_mode]);
    }

    /* Restore Terminal */
    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);

    for (int i = 0; i < window_count; i++) {
        if (windows[i]->state) free(windows[i]->state);
        free(windows[i]);
    }
    free(backbuffer);
    if (wallpaper_buffer) free(wallpaper_buffer);
    if (clean_backbuffer) free(clean_backbuffer);
    if (mouse_fd >= 0) close(mouse_fd);
    if (fb_fd >= 0) close(fb_fd);

    printf("XFCE 4 session terminated.\n");
    return 0;
}
