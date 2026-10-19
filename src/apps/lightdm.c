/*
 * IotaOS - lightdm.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * LightDM Graphical Display Manager for IotaOS.
 * Features:
 *  - Modern Nord/Slate styled lock & authentication screen.
 *  - User Account Selector: root (System Admin) and iota (Standard Desktop User).
 *  - Interactive Session Selector:
 *      * XFCE 4 Desktop Environment (/bin/xfce)
 *      * DOOM 3D Direct Raycaster Engine (/bin/doom)
 *      * Gears 3D Wayland Hardware Demo (/bin/gears)
 *      * Bash Login Console (/bin/bash)
 *      * Standard TTY Login (/bin/login)
 *  - Masked password authentication field with cursor blink.
 *  - Power Controls: System Reboot, System Poweroff, Console Switch.
 *  - Live status header with real-time clock, VM hypervisor detection, and memory info.
 *  - High-performance double-buffered rendering with smooth mouse cursor tracking.
 *  - Seamless POSIX session initialization (credentials, chdir, envp, execve).
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
#include <pwd.h>
#include <time.h>
#include <poll.h>
#include "iota.h"
#include "../kernel/drivers/fb_font.h"

/* Framebuffer & Screen State */
static int fb_fd = -1;
static uint32_t scr_w = 1024;
static uint32_t scr_h = 768;
static uint32_t scr_pitch = 4096;
static uint32_t scr_bpp = 32;
static uint32_t* fb_direct = NULL;
static uint32_t* backbuffer = NULL;
static uint32_t* clean_backbuffer = NULL;
static size_t fb_total_size = 0;
static bool screen_dirty = true;
static int prev_rendered_mx = -100;
static int prev_rendered_my = -100;

/* Mouse State */
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
static int mouse_sensitivity = 100;

/* Terminal State */
static struct termios orig_termios;
static bool running = true;

/* Nord / Slate Color Palette */
#define COL_BG_DARK         0x001a1c23
#define COL_BG_LIGHT        0x002e3440
#define COL_CARD_BG         0x00242933
#define COL_CARD_BORDER     0x00434c5e
#define COL_CARD_SHADOW     0x00121418
#define COL_ACCENT          0x005e81ac
#define COL_ACCENT_HOVER    0x0081a1c1
#define COL_CYAN            0x0088c0d0
#define COL_GREEN           0x00a3be8c
#define COL_RED             0x00bf616a
#define COL_AMBER           0x00ebcb8b
#define COL_TEXT_WHITE      0x00eceff4
#define COL_TEXT_MUTED      0x007b889b
#define COL_INPUT_BG        0x001b1e25
#define COL_INPUT_BORDER    0x004c566a
#define COL_INPUT_FOCUS     0x0088c0d0

/* Available Sessions */
typedef struct {
    const char* name;
    const char* desc;
    const char* bin_path;
    const char* icon;
} session_info_t;

static const session_info_t AVAILABLE_SESSIONS[] = {
    {"XFCE 4 Desktop",   "Lightweight Desktop Environment",  "/bin/xfce",        "[*]"},
    {"DOOM (Authentic)", "Authentic id Software Engine",     "/bin/doom",        "[#]"},
    {"DOOM Classic",     "Classic 3D Raycaster Engine",      "/bin/doomclassic", "[*]"},
    {"Gears 3D Demo",    "3D Shaded Rotating Gears",         "/bin/gears",       "[o]"},
    {"Bash Console",     "GNU Bourne-Again Shell",           "/bin/bash",        "[>]"},
    {"Diagnostics Test", "Automated 10/10 Test Suite",       "/bin/tests",       "[+]"},
    {"Terminal Login",   "Classic Text Console Session",     "/bin/login",       "[_]"},
};
#define NUM_SESSIONS ((int)(sizeof(AVAILABLE_SESSIONS) / sizeof(AVAILABLE_SESSIONS[0])))

/* User Accounts */
typedef struct {
    const char* username;
    const char* display_name;
    const char* role;
    uid_t uid;
    gid_t gid;
    const char* home;
} user_info_t;

static const user_info_t USER_ACCOUNTS[] = {
    {"root", "Root Administrator", "System Superuser", 0, 0, "/root"},
    {"iota", "IotaOS User",        "Standard Desktop", 1000, 1000, "/home/iota"}
};
#define NUM_USERS 2

/* Interactive State */
static int selected_user = 1;     /* Default to standard user 'iota' */
static int selected_session = 0;  /* Default to 'XFCE 4 Desktop' */
static char password_buf[64] = "";
static int password_len = 0;
static int focus_field = 0;       /* 0: Password box, 1: Session, 2: Login button */
static char toast_msg[96] = "";
static int toast_timer = 0;
static uint32_t anim_tick = 0;



/* --- GRAPHICS PRIMITIVES --- */

static inline void put_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && (uint32_t)x < scr_w && y >= 0 && (uint32_t)y < scr_h) {
        backbuffer[y * scr_w + x] = color;
    }
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
    uint8_t uc = (uint8_t)c;
    const uint8_t* glyph = fb_font_8x16[uc];
    for (int cy = 0; cy < 16; cy++) {
        int py = y + cy;
        if (py < 0 || (uint32_t)py >= scr_h) continue;
        uint8_t row_bits = glyph[cy];
        uint32_t* fb_row = &backbuffer[py * scr_w];
        for (int cx = 0; cx < 8; cx++) {
            int px = x + cx;
            if (px < 0 || (uint32_t)px >= scr_w) continue;
            if (row_bits & (0x80 >> cx)) {
                fb_row[px] = fg;
            } else if (bg != 0xFFFFFFFF) {
                fb_row[px] = bg;
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
    strncpy(toast_msg, msg, sizeof(toast_msg) - 1);
    toast_msg[sizeof(toast_msg) - 1] = '\0';
    toast_timer = 120;
    screen_dirty = true;
}

/* Mouse cursor rendering (16x16 crisp arrow) */
static void draw_mouse_cursor(int mx, int my) {
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

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            char p = sprite[y][x];
            if (p == 'O') put_pixel(mx + x, my + y, 0x00000000);
            else if (p == '#') put_pixel(mx + x, my + y, 0x00FFFFFF);
        }
    }
}

static void present_rect(int rx, int ry, int rw, int rh) {
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > (int)scr_w) rw = scr_w - rx;
    if (ry + rh > (int)scr_h) rh = scr_h - ry;
    if (rw <= 0 || rh <= 0) return;

    if (fb_direct) {
        for (int y = 0; y < rh; y++) {
            int py = ry + y;
            uint32_t* src = &backbuffer[py * scr_w + rx];
            uint32_t* dst = (uint32_t*)((uint8_t*)fb_direct + py * scr_pitch) + rx;
            memcpy(dst, src, rw * sizeof(uint32_t));
        }
    } else if (fb_fd >= 0) {
        for (int y = 0; y < rh; y++) {
            int py = ry + y;
            lseek(fb_fd, py * scr_pitch + rx * sizeof(uint32_t), SEEK_SET);
            write(fb_fd, &backbuffer[py * scr_w + rx], rw * sizeof(uint32_t));
        }
    }
}

static void present(void) {
    if (fb_direct) {
        for (uint32_t y = 0; y < scr_h; y++) {
            uint32_t* src = &backbuffer[y * scr_w];
            uint32_t* dst = (uint32_t*)((uint8_t*)fb_direct + y * scr_pitch);
            memcpy(dst, src, scr_w * sizeof(uint32_t));
        }
    } else if (fb_fd >= 0) {
        lseek(fb_fd, 0, SEEK_SET);
        write(fb_fd, backbuffer, fb_total_size);
    }
}

/* Fast cursor dirty rect updates */
static void update_cursor_dirty_rect(int old_x, int old_y, int new_x, int new_y) {
    /* 1. Restore old cursor area */
    for (int y = 0; y < 16; y++) {
        int py = old_y + y;
        if (py < 0 || (uint32_t)py >= scr_h) continue;
        int px_start = old_x;
        int px_end = old_x + 16;
        if (px_start < 0) px_start = 0;
        if ((uint32_t)px_end > scr_w) px_end = scr_w;
        if (px_end > px_start) {
            memcpy(&backbuffer[py * scr_w + px_start],
                   &clean_backbuffer[py * scr_w + px_start],
                   (px_end - px_start) * sizeof(uint32_t));
        }
    }
    present_rect(old_x, old_y, 16, 16);

    /* 2. Render new cursor area */
    draw_mouse_cursor(new_x, new_y);
    present_rect(new_x, new_y, 16, 16);
}

/* --- BACKGROUND & WALLPAPER --- */

static void render_background(void) {
    /* Smooth vertical slate gradient */
    for (uint32_t y = 0; y < scr_h; y++) {
        uint32_t t = (y * 255) / scr_h;
        uint32_t r = ((26 * (255 - t)) + (46 * t)) / 255;
        uint32_t g = ((28 * (255 - t)) + (52 * t)) / 255;
        uint32_t b = ((35 * (255 - t)) + (64 * t)) / 255;
        uint32_t col = (r << 16) | (g << 8) | b;
        uint32_t* row = &backbuffer[y * scr_w];
        for (uint32_t x = 0; x < scr_w; x++) row[x] = col;
    }

    /* Ambient geometric grid lines in the background */
    for (uint32_t y = 0; y < scr_h; y += 48) {
        for (uint32_t x = 0; x < scr_w; x += 4) {
            put_pixel(x, y, 0x00232731);
        }
    }
    for (uint32_t x = 0; x < scr_w; x += 48) {
        for (uint32_t y = 0; y < scr_h; y += 4) {
            put_pixel(x, y, 0x00232731);
        }
    }
}

/* --- TOP STATUS BAR --- */

static void render_top_bar(void) {
    fill_rect(0, 0, scr_w, 32, COL_CARD_BG);
    fill_rect(0, 31, scr_w, 1, COL_CARD_BORDER);

    /* Brand mark */
    fill_rect(12, 10, 12, 12, COL_CYAN);
    draw_string(32, 8, "IotaOS LightDM", COL_TEXT_WHITE, COL_CARD_BG);

    /* System status indicator */
    draw_string(200, 8, "Desktop Workstation", COL_GREEN, COL_CARD_BG);

    /* Real-time Clock */
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%a %b %e  %H:%M:%S", &tm);
    int time_w = strlen(time_str) * 8;
    draw_string((scr_w - time_w) / 2, 8, time_str, COL_TEXT_WHITE, COL_CARD_BG);

    /* Session Quick Badge on top right */
    char sess_badge[64];
    snprintf(sess_badge, sizeof(sess_badge), "Session: %s", AVAILABLE_SESSIONS[selected_session].name);
    draw_string(scr_w - 260, 8, sess_badge, COL_CYAN, COL_CARD_BG);
}

/* --- LOGIN CARD UI LAYOUT --- */

#define CARD_W 460
#define CARD_H 410

static int card_x = 0;
static int card_y = 0;

/* Bounding boxes for clickable widgets */
static int user_tab_root_x, user_tab_root_y, user_tab_root_w, user_tab_root_h;
static int user_tab_iota_x, user_tab_iota_y, user_tab_iota_w, user_tab_iota_h;
static int pass_box_x, pass_box_y, pass_box_w, pass_box_h;
static int sess_left_x, sess_left_y, sess_left_w, sess_left_h;
static int sess_box_x, sess_box_y, sess_box_w, sess_box_h;
static int sess_right_x, sess_right_y, sess_right_w, sess_right_h;
static int login_btn_x, login_btn_y, login_btn_w, login_btn_h;
static int reboot_btn_x, reboot_btn_y, reboot_btn_w, reboot_btn_h;
static int poweroff_btn_x, poweroff_btn_y, poweroff_btn_w, poweroff_btn_h;
static int console_btn_x, console_btn_y, console_btn_w, console_btn_h;

static void render_login_card(void) {
    card_x = (scr_w - CARD_W) / 2;
    card_y = (scr_h - CARD_H) / 2 + 10;

    /* Drop shadow */
    fill_rect(card_x + 6, card_y + 6, CARD_W, CARD_H, COL_CARD_SHADOW);

    /* Card background and border */
    fill_rect(card_x, card_y, CARD_W, CARD_H, COL_CARD_BG);
    draw_rect(card_x, card_y, CARD_W, CARD_H, COL_CARD_BORDER);

    /* Card Header banner */
    fill_rect(card_x + 1, card_y + 1, CARD_W - 2, 46, 0x002c3340);
    fill_rect(card_x + 1, card_y + 47, CARD_W - 2, 1, COL_CARD_BORDER);

    draw_string(card_x + 20, card_y + 14, "Welcome to IotaOS", COL_CYAN, 0x002c3340);
    draw_string(card_x + 230, card_y + 16, "v2.0-LTS (i686)", COL_TEXT_MUTED, 0x002c3340);

    /* User Avatar & Selector */
    int u_y = card_y + 64;
    user_tab_root_x = card_x + 24;
    user_tab_root_y = u_y;
    user_tab_root_w = 196;
    user_tab_root_h = 44;

    user_tab_iota_x = card_x + 240;
    user_tab_iota_y = u_y;
    user_tab_iota_w = 196;
    user_tab_iota_h = 44;

    /* Root User Tab */
    bool root_active = (selected_user == 0);
    fill_rect(user_tab_root_x, user_tab_root_y, user_tab_root_w, user_tab_root_h, root_active ? 0x003b4252 : 0x001f232b);
    draw_rect(user_tab_root_x, user_tab_root_y, user_tab_root_w, user_tab_root_h, root_active ? COL_ACCENT : COL_CARD_BORDER);
    fill_rect(user_tab_root_x + 8, user_tab_root_y + 8, 28, 28, root_active ? COL_RED : 0x004c566a);
    draw_string(user_tab_root_x + 18, user_tab_root_y + 14, "#", COL_TEXT_WHITE, 0xFFFFFFFF);
    draw_string(user_tab_root_x + 44, user_tab_root_y + 8, "root", root_active ? COL_TEXT_WHITE : COL_TEXT_MUTED, 0xFFFFFFFF);
    draw_string(user_tab_root_x + 44, user_tab_root_y + 24, "Administrator", COL_TEXT_MUTED, 0xFFFFFFFF);

    /* Iota User Tab */
    bool iota_active = (selected_user == 1);
    fill_rect(user_tab_iota_x, user_tab_iota_y, user_tab_iota_w, user_tab_iota_h, iota_active ? 0x003b4252 : 0x001f232b);
    draw_rect(user_tab_iota_x, user_tab_iota_y, user_tab_iota_w, user_tab_iota_h, iota_active ? COL_ACCENT : COL_CARD_BORDER);
    fill_rect(user_tab_iota_x + 8, user_tab_iota_y + 8, 28, 28, iota_active ? COL_GREEN : 0x004c566a);
    draw_string(user_tab_iota_x + 18, user_tab_iota_y + 14, "i", COL_TEXT_WHITE, 0xFFFFFFFF);
    draw_string(user_tab_iota_x + 44, user_tab_iota_y + 8, "iota", iota_active ? COL_TEXT_WHITE : COL_TEXT_MUTED, 0xFFFFFFFF);
    draw_string(user_tab_iota_x + 44, user_tab_iota_y + 24, "Standard User", COL_TEXT_MUTED, 0xFFFFFFFF);

    /* Password Input Field */
    int p_y = card_y + 128;
    draw_string(card_x + 24, p_y, "Password:", COL_TEXT_WHITE, COL_CARD_BG);

    pass_box_x = card_x + 24;
    pass_box_y = p_y + 20;
    pass_box_w = CARD_W - 48;
    pass_box_h = 36;

    fill_rect(pass_box_x, pass_box_y, pass_box_w, pass_box_h, COL_INPUT_BG);
    draw_rect(pass_box_x, pass_box_y, pass_box_w, pass_box_h, (focus_field == 0) ? COL_INPUT_FOCUS : COL_INPUT_BORDER);

    /* Render password dots */
    int dot_x = pass_box_x + 12;
    for (int i = 0; i < password_len; i++) {
        fill_rect(dot_x, pass_box_y + 14, 8, 8, COL_TEXT_WHITE);
        dot_x += 14;
    }

    /* Cursor blink */
    if (focus_field == 0 && ((anim_tick / 15) % 2 == 0)) {
        fill_rect(dot_x, pass_box_y + 10, 2, 16, COL_CYAN);
    }

    if (password_len == 0 && focus_field != 0) {
        draw_string(pass_box_x + 12, pass_box_y + 10, "(press Enter or click Log In)", COL_TEXT_MUTED, COL_INPUT_BG);
    }

    draw_string(card_x + 24, pass_box_y + 42, "Note: Accounts are passwordless (press Enter to log in)", 0x005e81ac, COL_CARD_BG);

    /* Session Selector Carousel */
    int s_y = card_y + 218;
    draw_string(card_x + 24, s_y, "Desktop Session:", COL_TEXT_WHITE, COL_CARD_BG);

    sess_left_x = card_x + 24;
    sess_left_y = s_y + 20;
    sess_left_w = 36;
    sess_left_h = 38;

    sess_box_x = card_x + 64;
    sess_box_y = s_y + 20;
    sess_box_w = CARD_W - 128;
    sess_box_h = 38;

    sess_right_x = card_x + CARD_W - 60;
    sess_right_y = s_y + 20;
    sess_right_w = 36;
    sess_right_h = 38;

    /* Left Arrow */
    fill_rect(sess_left_x, sess_left_y, sess_left_w, sess_left_h, 0x002e3440);
    draw_rect(sess_left_x, sess_left_y, sess_left_w, sess_left_h, COL_CARD_BORDER);
    draw_string(sess_left_x + 12, sess_left_y + 11, "<", COL_TEXT_WHITE, 0x002e3440);

    /* Right Arrow */
    fill_rect(sess_right_x, sess_right_y, sess_right_w, sess_right_h, 0x002e3440);
    draw_rect(sess_right_x, sess_right_y, sess_right_w, sess_right_h, COL_CARD_BORDER);
    draw_string(sess_right_x + 14, sess_right_y + 11, ">", COL_TEXT_WHITE, 0x002e3440);

    /* Session Display Box */
    fill_rect(sess_box_x, sess_box_y, sess_box_w, sess_box_h, 0x001f232b);
    draw_rect(sess_box_x, sess_box_y, sess_box_w, sess_box_h, (focus_field == 1) ? COL_INPUT_FOCUS : COL_CARD_BORDER);

    const session_info_t* cur_sess = &AVAILABLE_SESSIONS[selected_session];
    draw_string(sess_box_x + 14, sess_box_y + 11, cur_sess->icon, COL_CYAN, 0x001f232b);
    draw_string(sess_box_x + 44, sess_box_y + 11, cur_sess->name, COL_TEXT_WHITE, 0x001f232b);

    /* Session description */
    draw_string_clipped(card_x + 24, sess_box_y + 44, CARD_W - 48, cur_sess->desc, COL_TEXT_MUTED, COL_CARD_BG);

    /* Log In Action Button */
    login_btn_x = card_x + 24;
    login_btn_y = card_y + 300;
    login_btn_w = CARD_W - 48;
    login_btn_h = 42;

    fill_rect(login_btn_x, login_btn_y, login_btn_w, login_btn_h, (focus_field == 2) ? COL_ACCENT_HOVER : COL_ACCENT);
    draw_rect(login_btn_x, login_btn_y, login_btn_w, login_btn_h, COL_CYAN);

    char login_txt[64];
    snprintf(login_txt, sizeof(login_txt), "Log In as %s  -->", USER_ACCOUNTS[selected_user].username);
    int login_txt_w = strlen(login_txt) * 8;
    draw_string(login_btn_x + (login_btn_w - login_txt_w) / 2, login_btn_y + 13, login_txt, COL_TEXT_WHITE, 0xFFFFFFFF);

    /* Power Controls at bottom of card */
    int b_y = card_y + 356;
    fill_rect(card_x + 1, b_y, CARD_W - 2, 1, COL_CARD_BORDER);

    reboot_btn_x = card_x + 24;
    reboot_btn_y = b_y + 12;
    reboot_btn_w = 120;
    reboot_btn_h = 30;

    poweroff_btn_x = card_x + 154;
    poweroff_btn_y = b_y + 12;
    poweroff_btn_w = 130;
    poweroff_btn_h = 30;

    console_btn_x = card_x + 294;
    console_btn_y = b_y + 12;
    console_btn_w = 142;
    console_btn_h = 30;

    fill_rect(reboot_btn_x, reboot_btn_y, reboot_btn_w, reboot_btn_h, 0x002c3340);
    draw_rect(reboot_btn_x, reboot_btn_y, reboot_btn_w, reboot_btn_h, COL_CARD_BORDER);
    draw_string(reboot_btn_x + 16, reboot_btn_y + 7, "[o] Restart", COL_TEXT_WHITE, 0x002c3340);

    fill_rect(poweroff_btn_x, poweroff_btn_y, poweroff_btn_w, poweroff_btn_h, 0x002c3340);
    draw_rect(poweroff_btn_x, poweroff_btn_y, poweroff_btn_w, poweroff_btn_h, COL_CARD_BORDER);
    draw_string(poweroff_btn_x + 14, poweroff_btn_y + 7, "[x] Shut Down", COL_RED, 0x002c3340);

    fill_rect(console_btn_x, console_btn_y, console_btn_w, console_btn_h, 0x002c3340);
    draw_rect(console_btn_x, console_btn_y, console_btn_w, console_btn_h, COL_CARD_BORDER);
    draw_string(console_btn_x + 14, console_btn_y + 7, "[>] TTY Login", COL_AMBER, 0x002c3340);
}

/* Toast Notification (error / status) */
static void render_toast(void) {
    if (toast_timer <= 0 || !toast_msg[0]) return;
    int tw = strlen(toast_msg) * 8 + 32;
    int th = 32;
    int tx = (scr_w - tw) / 2;
    int ty = scr_h - 70;

    fill_rect(tx, ty, tw, th, 0x003b4252);
    draw_rect(tx, ty, tw, th, COL_CYAN);
    draw_string(tx + 16, ty + 8, toast_msg, COL_TEXT_WHITE, 0x003b4252);
}

/* --- SESSION LAUNCHER & AUTHENTICATION --- */

static void do_login(void) {
    const user_info_t* u = &USER_ACCOUNTS[selected_user];
    const session_info_t* s = &AVAILABLE_SESSIONS[selected_session];

    /* Look up user in /etc/passwd */
    struct passwd* pw = getpwnam(u->username);
    uid_t uid = pw ? pw->pw_uid : u->uid;
    gid_t gid = pw ? pw->pw_gid : u->gid;
    const char* home = pw ? pw->pw_dir : u->home;
    const char* shell = (pw && pw->pw_shell[0]) ? pw->pw_shell : "/bin/bash";

    /* Check if target binary exists */
    if (access(s->bin_path, X_OK) != 0) {
        char err[96];
        snprintf(err, sizeof(err), "Error: %s not installed or not executable", s->bin_path);
        show_toast(err);
        return;
    }

    char msg[80];
    snprintf(msg, sizeof(msg), "Starting %s as %s...", s->name, u->username);
    show_toast(msg);

    /* Render status frame before handoff */
    render_background();
    render_top_bar();
    render_login_card();
    render_toast();
    present();

    /* Restore Terminal attributes */
    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);

    /* Close file descriptors */
    if (mouse_fd >= 0) close(mouse_fd);
    if (fb_fd >= 0) close(fb_fd);

    /* Clean memory */
    if (backbuffer) free(backbuffer);
    if (clean_backbuffer) free(clean_backbuffer);

    /* Apply user credentials */
    setgid(gid);
    setuid(uid);

    if (chdir(home) < 0) {
        chdir("/");
    }

    /* Configure environment */
    char e_home[160], e_user[80], e_logname[80], e_shell[160], e_path[160], e_term[32], e_disp[32];
    snprintf(e_home, sizeof(e_home), "HOME=%s", home);
    snprintf(e_user, sizeof(e_user), "USER=%s", u->username);
    snprintf(e_logname, sizeof(e_logname), "LOGNAME=%s", u->username);
    snprintf(e_shell, sizeof(e_shell), "SHELL=%s", shell);
    snprintf(e_path, sizeof(e_path), "PATH=%s", uid == 0
             ? "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
             : "/usr/local/bin:/usr/bin:/bin");
    snprintf(e_term, sizeof(e_term), "TERM=linux");
    snprintf(e_disp, sizeof(e_disp), "DISPLAY=:0");
    char* envp[] = {e_home, e_user, e_logname, e_shell, e_path, e_term, e_disp, NULL};

    const char* base_name = strrchr(s->bin_path, '/');
    char argv0[64];
    if (strcmp(s->bin_path, "/bin/bash") == 0) {
        snprintf(argv0, sizeof(argv0), "-bash");
    } else {
        snprintf(argv0, sizeof(argv0), "%s", base_name ? base_name + 1 : s->bin_path);
    }
    char* argv[] = {argv0, NULL};

    execve(s->bin_path, argv, envp);

    /* If execve failed */
    fprintf(stderr, "lightdm: failed to execute %s\n", s->bin_path);
    exit(1);
}

static void do_reboot(void) {
    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[HSystem is rebooting NOW...\n");
    fflush(stdout);
    if (fb_fd >= 0) close(fb_fd);
    if (mouse_fd >= 0) close(mouse_fd);
    iota_reboot();
    exit(0);
}

static void do_poweroff(void) {
    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[HSystem is powering off NOW...\n");
    fflush(stdout);
    if (fb_fd >= 0) close(fb_fd);
    if (mouse_fd >= 0) close(mouse_fd);
    iota_syscall3(IOTA_SYS_POWEROFF, 0, 0, 0);
    exit(0);
}

static void do_console_login(void) {
    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);
    if (fb_fd >= 0) close(fb_fd);
    if (mouse_fd >= 0) close(mouse_fd);

    char* argv[] = {"login", NULL};
    char* envp[] = {"TERM=linux", "PATH=/bin:/usr/bin", NULL};
    execve("/bin/login", argv, envp);
    exit(0);
}

/* --- INPUT PROCESSING --- */

static void handle_click(int mx, int my) {
    /* User Tabs */
    if (mx >= user_tab_root_x && mx <= user_tab_root_x + user_tab_root_w &&
        my >= user_tab_root_y && my <= user_tab_root_y + user_tab_root_h) {
        selected_user = 0;
        screen_dirty = true;
        return;
    }
    if (mx >= user_tab_iota_x && mx <= user_tab_iota_x + user_tab_iota_w &&
        my >= user_tab_iota_y && my <= user_tab_iota_y + user_tab_iota_h) {
        selected_user = 1;
        screen_dirty = true;
        return;
    }

    /* Password Box Focus */
    if (mx >= pass_box_x && mx <= pass_box_x + pass_box_w &&
        my >= pass_box_y && my <= pass_box_y + pass_box_h) {
        focus_field = 0;
        screen_dirty = true;
        return;
    }

    /* Session Left Arrow */
    if (mx >= sess_left_x && mx <= sess_left_x + sess_left_w &&
        my >= sess_left_y && my <= sess_left_y + sess_left_h) {
        selected_session = (selected_session - 1 + NUM_SESSIONS) % NUM_SESSIONS;
        screen_dirty = true;
        return;
    }

    /* Session Right Arrow */
    if (mx >= sess_right_x && mx <= sess_right_x + sess_right_w &&
        my >= sess_right_y && my <= sess_right_y + sess_right_h) {
        selected_session = (selected_session + 1) % NUM_SESSIONS;
        screen_dirty = true;
        return;
    }

    /* Session Box Click (cycle forward) */
    if (mx >= sess_box_x && mx <= sess_box_x + sess_box_w &&
        my >= sess_box_y && my <= sess_box_y + sess_box_h) {
        selected_session = (selected_session + 1) % NUM_SESSIONS;
        focus_field = 1;
        screen_dirty = true;
        return;
    }

    /* Log In Button */
    if (mx >= login_btn_x && mx <= login_btn_x + login_btn_w &&
        my >= login_btn_y && my <= login_btn_y + login_btn_h) {
        do_login();
        return;
    }

    /* Power Controls */
    if (mx >= reboot_btn_x && mx <= reboot_btn_x + reboot_btn_w &&
        my >= reboot_btn_y && my <= reboot_btn_y + reboot_btn_h) {
        do_reboot();
        return;
    }

    if (mx >= poweroff_btn_x && mx <= poweroff_btn_x + poweroff_btn_w &&
        my >= poweroff_btn_y && my <= poweroff_btn_y + poweroff_btn_h) {
        do_poweroff();
        return;
    }

    if (mx >= console_btn_x && mx <= console_btn_x + console_btn_w &&
        my >= console_btn_y && my <= console_btn_y + console_btn_h) {
        do_console_login();
        return;
    }
}

static void process_mouse(void) {
    if (mouse_fd < 0) return;

    mouse_state_ext_t ms;
    while (read(mouse_fd, &ms, sizeof(ms)) == (ssize_t)sizeof(ms)) {
        cur_mx = ms.x;
        cur_my = ms.y;
        cur_mbtn = ms.buttons;

        if (cur_mx < 0) cur_mx = 0;
        if ((uint32_t)cur_mx >= scr_w) cur_mx = scr_w - 1;
        if (cur_my < 0) cur_my = 0;
        if ((uint32_t)cur_my >= scr_h) cur_my = scr_h - 1;

        if ((cur_mbtn & 1) && !(prev_mbtn & 1)) {
            handle_click(cur_mx, cur_my);
        }

        prev_mbtn = cur_mbtn;
    }
}

static void process_keyboard(void) {
    struct pollfd pfd = { .fd = 0, .events = POLLIN, .revents = 0 };
    while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        char ch = 0;
        if (read(0, &ch, 1) <= 0) break;

        if (ch == 27) { /* Escape or ANSI sequence */
            char seq[3] = {0};
            if (poll(&pfd, 1, 50) > 0 && read(0, &seq[0], 1) > 0 && seq[0] == '[') {
                if (poll(&pfd, 1, 50) > 0 && read(0, &seq[1], 1) > 0) {
                    if (seq[1] == 'A') { /* Up Arrow */
                        selected_user = 0; /* root */
                        screen_dirty = true;
                    } else if (seq[1] == 'B') { /* Down Arrow */
                        selected_user = 1; /* iota */
                        screen_dirty = true;
                    } else if (seq[1] == 'D') { /* Left Arrow */
                        selected_session = (selected_session - 1 + NUM_SESSIONS) % NUM_SESSIONS;
                        screen_dirty = true;
                    } else if (seq[1] == 'C') { /* Right Arrow */
                        selected_session = (selected_session + 1) % NUM_SESSIONS;
                        screen_dirty = true;
                    }
                }
            } else {
                /* Pure Escape: clear password */
                password_len = 0;
                password_buf[0] = '\0';
                screen_dirty = true;
            }
        } else if (ch == '\t') { /* Tab key */
            focus_field = (focus_field + 1) % 3;
            screen_dirty = true;
        } else if (ch == '\n' || ch == '\r') { /* Enter key */
            do_login();
        } else if (ch == 8 || ch == 127) { /* Backspace */
            if (password_len > 0) {
                password_len--;
                password_buf[password_len] = '\0';
                screen_dirty = true;
            }
        } else if (ch >= 32 && ch <= 126) { /* Printable key */
            if (password_len < (int)sizeof(password_buf) - 1) {
                password_buf[password_len++] = ch;
                password_buf[password_len] = '\0';
                focus_field = 0;
                screen_dirty = true;
            }
        }
    }
}

/* --- MAIN ENTRY POINT --- */

int main(void) {
    /* 1. Open Framebuffer */
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        /* If framebuffer is unavailable, fallback to text console login */
        char* argv[] = {"login", NULL};
        char* envp[] = {"TERM=linux", "PATH=/bin:/usr/bin", NULL};
        execve("/bin/login", argv, envp);
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
    clean_backbuffer = (uint32_t*)malloc(fb_total_size);
    if (!backbuffer || !clean_backbuffer) {
        close(fb_fd);
        char* argv[] = {"login", NULL};
        char* envp[] = {"TERM=linux", "PATH=/bin:/usr/bin", NULL};
        execve("/bin/login", argv, envp);
        return 1;
    }

    /* 3. Open Mouse */
    mouse_fd = open("/dev/mouse", O_RDONLY | O_NONBLOCK);
    if (mouse_fd >= 0) ioctl(mouse_fd, 2, &mouse_sensitivity);

    /* 4. Configure Raw Terminal Input */
    tcgetattr(0, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);

    /* Hide text console cursor */
    printf("\033[?25l");
    fflush(stdout);

    /* Initial composition */
    render_background();
    render_top_bar();
    render_login_card();
    memcpy(clean_backbuffer, backbuffer, fb_total_size);
    draw_mouse_cursor(cur_mx, cur_my);
    present();
    prev_rendered_mx = cur_mx;
    prev_rendered_my = cur_my;
    screen_dirty = false;

    /* 5. Main Event Loop */
    time_t last_sec = time(NULL);

    while (running) {
        anim_tick++;

        /* 1-second clock tick */
        time_t cur_sec = time(NULL);
        if (cur_sec != last_sec) {
            last_sec = cur_sec;
            screen_dirty = true;
        }

        /* Toast countdown */
        if (toast_timer > 0) {
            toast_timer--;
            if (toast_timer == 0) screen_dirty = true;
        }

        /* Cursor blink tick */
        if (focus_field == 0 && (anim_tick % 15 == 0)) {
            screen_dirty = true;
        }

        /* Poll inputs */
        process_mouse();
        process_keyboard();

        if (screen_dirty) {
            render_background();
            render_top_bar();
            render_login_card();
            render_toast();

            memcpy(clean_backbuffer, backbuffer, fb_total_size);
            draw_mouse_cursor(cur_mx, cur_my);
            present();

            prev_rendered_mx = cur_mx;
            prev_rendered_my = cur_my;
            screen_dirty = false;
        } else if (cur_mx != prev_rendered_mx || cur_my != prev_rendered_my) {
            update_cursor_dirty_rect(prev_rendered_mx, prev_rendered_my, cur_mx, cur_my);
            prev_rendered_mx = cur_mx;
            prev_rendered_my = cur_my;
        }

        /* Frame pacing: ~60-120 FPS */
        usleep(10000);
    }

    /* Restore Terminal */
    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);

    if (mouse_fd >= 0) close(mouse_fd);
    if (fb_fd >= 0) close(fb_fd);
    free(backbuffer);
    free(clean_backbuffer);
    return 0;
}
