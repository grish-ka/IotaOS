/*
 * IotaOS - gears.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * Classic 3D Rotating Gears demo for IotaOS (inspired by glxgears / wayland-gears).
 * Features:
 *  - 3 interlocking rotating gears (Red, Green, Blue) with exact meshing ratios.
 *  - 3D directional lighting and bevel shading.
 *  - Live FPS and frame-time benchmark meter.
 *  - Interactive speed controls, color themes, and pause/resume.
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
#include <poll.h>
#include <time.h>
#include <math.h>
#include "iota.h"
#include "../kernel/drivers/fb_font.h"

#define M_PI 3.14159265358979323846

static int fb_fd = -1;
static uint32_t scr_w = 1024;
static uint32_t scr_h = 768;
static uint32_t scr_pitch = 4096;
static uint32_t* fb_direct = NULL;
static uint32_t* backbuffer = NULL;
static size_t fb_total_size = 0;
static struct termios orig_termios;

/* Color Palettes */
static const uint32_t PAL_CLASSIC[3] = { 0x00E03030, 0x0030B040, 0x002070D0 }; /* Red, Green, Blue */
static const uint32_t PAL_NEON[3]    = { 0x00FF007F, 0x0000FFCC, 0x00FFEE00 }; /* Cyberpunk */
static const uint32_t PAL_BREEZE[3]  = { 0x003DAEE9, 0x002ECC71, 0x009B59B6 }; /* KDE Breeze */
static const uint32_t PAL_BRONZE[3]  = { 0x00D4AF37, 0x00CD7F32, 0x00B8860B }; /* Metallic */
static int current_palette = 0;

typedef struct {
    float cx, cy;          /* Center coordinate */
    float inner_r;         /* Axle hole radius */
    float root_r;          /* Hub radius (base of teeth) */
    float outer_r;         /* Tip of teeth radius */
    int teeth;             /* Number of teeth */
    float speed_mult;      /* Gear ratio */
    float phase_offset;    /* Initial angle offset for perfect meshing */
    int pal_idx;           /* Base color index */
} gear_t;

static gear_t gears[3];

static void init_gears(void) {
    float mid_x = scr_w / 2.0f;
    float mid_y = scr_h / 2.0f - 20.0f;

    /* Gear 0: Red Gear (large, 20 teeth, driven gear) */
    gears[0].cx = mid_x - 130.0f;
    gears[0].cy = mid_y - 80.0f;
    gears[0].inner_r = 25.0f;
    gears[0].root_r  = 85.0f;
    gears[0].outer_r = 110.0f;
    gears[0].teeth   = 20;
    gears[0].speed_mult = 1.0f;
    gears[0].phase_offset = 0.0f;
    gears[0].pal_idx = 0;

    /* Gear 1: Green Gear (medium, 14 teeth, meshed at bottom-right) */
    float d01 = gears[0].root_r + 14.0f + 65.0f;
    float ang01 = 0.85f; /* Radians */
    gears[1].cx = gears[0].cx + cosf(ang01) * d01;
    gears[1].cy = gears[0].cy + sinf(ang01) * d01;
    gears[1].inner_r = 18.0f;
    gears[1].root_r  = 65.0f;
    gears[1].outer_r = 85.0f;
    gears[1].teeth   = 14;
    gears[1].speed_mult = -(float)gears[0].teeth / (float)gears[1].teeth;
    gears[1].phase_offset = 0.18f;
    gears[1].pal_idx = 1;

    /* Gear 2: Blue Gear (small, 10 teeth, meshed at top-right of Red Gear) */
    float d02 = gears[0].root_r + 14.0f + 48.0f;
    float ang02 = -0.42f;
    gears[2].cx = gears[0].cx + cosf(ang02) * d02;
    gears[2].cy = gears[0].cy + sinf(ang02) * d02;
    gears[2].inner_r = 14.0f;
    gears[2].root_r  = 48.0f;
    gears[2].outer_r = 65.0f;
    gears[2].teeth   = 10;
    gears[2].speed_mult = -(float)gears[0].teeth / (float)gears[2].teeth;
    gears[2].phase_offset = 0.25f;
    gears[2].pal_idx = 2;
}

static inline void put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || (uint32_t)x >= scr_w || y < 0 || (uint32_t)y >= scr_h) return;
    backbuffer[y * scr_w + x] = color;
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
    while (*str) {
        draw_char(x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)scr_w) w = (int)scr_w - x;
    if (y + h > (int)scr_h) h = (int)scr_h - y;
    if (w <= 0 || h <= 0) return;

    for (int r = 0; r < h; r++) {
        uint32_t* row = &backbuffer[(y + r) * scr_w + x];
        for (int c = 0; c < w; c++) row[c] = color;
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

/* Precomputed Look-Up Tables (LUT) for 3D Gears */
#define GEAR_LUT_MAX_R 112
#define GEAR_LUT_DIM (GEAR_LUT_MAX_R * 2 + 1)
static uint8_t gear_lut_dist[GEAR_LUT_DIM][GEAR_LUT_DIM];
static uint16_t gear_lut_angle[GEAR_LUT_DIM][GEAR_LUT_DIM];
static uint8_t gear_lut_light[GEAR_LUT_DIM][GEAR_LUT_DIM];
static bool gear_lut_inited = false;

static void init_gear_lut(void) {
    if (gear_lut_inited) return;
    for (int dy = 0; dy <= GEAR_LUT_MAX_R; dy++) {
        float fdy = (float)dy;
        for (int dx = 0; dx <= GEAR_LUT_MAX_R; dx++) {
            float fdx = (float)dx;
            float dist = sqrtf(fdx * fdx + fdy * fdy);
            uint8_t d_val = (uint8_t)(dist > 255.0f ? 255 : (uint8_t)(dist + 0.5f));

            float theta0 = atan2f(fdy, fdx); /* Quadrant 1: [0, pi/2] */
            float theta1 = (float)M_PI - theta0; /* Quadrant 2: [pi/2, pi] */
            float theta2 = (float)M_PI + theta0; /* Quadrant 3: [pi, 3pi/2] */
            float theta3 = 2.0f * (float)M_PI - theta0; /* Quadrant 4: [3pi/2, 2pi] */

            float inv_dist = 1.0f / (dist + 0.001f);
            float nx = fdx * inv_dist;
            float ny = fdy * inv_dist;

            /* Quadrant (+dx, +dy) */
            int ix = dx + GEAR_LUT_MAX_R, iy = dy + GEAR_LUT_MAX_R;
            gear_lut_dist[iy][ix] = d_val;
            gear_lut_angle[iy][ix] = (uint16_t)(((uint32_t)((theta0 / (2.0f * (float)M_PI)) * 65536.0f)) & 0xFFFF);
            float l0 = 0.85f - (nx * 0.35f + ny * 0.40f);
            gear_lut_light[iy][ix] = (uint8_t)((l0 < 0.25f ? 0.25f : (l0 > 1.4f ? 1.4f : l0)) * 128.0f);

            /* Quadrant (-dx, +dy) */
            ix = -dx + GEAR_LUT_MAX_R;
            gear_lut_dist[iy][ix] = d_val;
            gear_lut_angle[iy][ix] = (uint16_t)(((uint32_t)((theta1 / (2.0f * (float)M_PI)) * 65536.0f)) & 0xFFFF);
            float l1 = 0.85f - (-nx * 0.35f + ny * 0.40f);
            gear_lut_light[iy][ix] = (uint8_t)((l1 < 0.25f ? 0.25f : (l1 > 1.4f ? 1.4f : l1)) * 128.0f);

            /* Quadrant (-dx, -dy) */
            iy = -dy + GEAR_LUT_MAX_R;
            gear_lut_dist[iy][ix] = d_val;
            gear_lut_angle[iy][ix] = (uint16_t)(((uint32_t)((theta2 / (2.0f * (float)M_PI)) * 65536.0f)) & 0xFFFF);
            float l2 = 0.85f - (-nx * 0.35f - ny * 0.40f);
            gear_lut_light[iy][ix] = (uint8_t)((l2 < 0.25f ? 0.25f : (l2 > 1.4f ? 1.4f : l2)) * 128.0f);

            /* Quadrant (+dx, -dy) */
            ix = dx + GEAR_LUT_MAX_R;
            gear_lut_dist[iy][ix] = d_val;
            gear_lut_angle[iy][ix] = (uint16_t)(((uint32_t)((theta3 / (2.0f * (float)M_PI)) * 65536.0f)) & 0xFFFF);
            float l3 = 0.85f - (nx * 0.35f - ny * 0.40f);
            gear_lut_light[iy][ix] = (uint8_t)((l3 < 0.25f ? 0.25f : (l3 > 1.4f ? 1.4f : l3)) * 128.0f);
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

/* Render a single gear with 3D tooth geometry and directional shading */
static void render_gear(const gear_t* g, float angle) {
    const uint32_t* pal = PAL_CLASSIC;
    if (current_palette == 1) pal = PAL_NEON;
    else if (current_palette == 2) pal = PAL_BREEZE;
    else if (current_palette == 3) pal = PAL_BRONZE;

    uint32_t base_col = pal[g->pal_idx];

    while (angle < 0.0f) angle += 2.0f * (float)M_PI;
    while (angle >= 2.0f * (float)M_PI) angle -= 2.0f * (float)M_PI;
    uint16_t ang_fixed = (uint16_t)((angle / (2.0f * (float)M_PI)) * 65536.0f);

    int cx_i = (int)g->cx;
    int cy_i = (int)g->cy;
    int r_in = (int)g->inner_r;
    int r_root = (int)g->root_r;
    int r_out = (int)g->outer_r;
    uint32_t period = 65536 / g->teeth;
    uint32_t t_min = (period * 15) / 100;
    uint32_t t_max = (period * 65) / 100;

    int min_x = cx_i - r_out;
    int max_x = cx_i + r_out;
    int min_y = cy_i - r_out;
    int max_y = cy_i + r_out;

    if (min_x < 0) min_x = 0;
    if ((uint32_t)max_x >= scr_w) max_x = scr_w - 1;
    if (min_y < 0) min_y = 0;
    if ((uint32_t)max_y >= scr_h) max_y = scr_h - 1;

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

            put_pixel(px, py, fast_shade(base_col, row_light[lut_x]));
        }
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    /* 1. Open Framebuffer */
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "gears: cannot open /dev/fb0\n");
        return 1;
    }

    uint32_t fb_meta[6] = {0};
    if (ioctl(fb_fd, 0x4600, fb_meta) == 0 && fb_meta[0] > 0 && fb_meta[1] > 0) {
        scr_w = fb_meta[0];
        scr_h = fb_meta[1];
        scr_pitch = fb_meta[2];
        fb_direct = (uint32_t*)fb_meta[4];
        fb_total_size = fb_meta[5];
    } else {
        scr_w = 1024;
        scr_h = 768;
        scr_pitch = 4096;
        fb_total_size = scr_pitch * scr_h;
    }

    backbuffer = (uint32_t*)malloc(fb_total_size);
    if (!backbuffer) {
        fprintf(stderr, "gears: out of memory for backbuffer\n");
        close(fb_fd);
        return 1;
    }

    /* 2. Configure terminal input */
    tcgetattr(0, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);
    fcntl(0, F_SETFL, O_NONBLOCK);

    printf("\033[?25l");
    fflush(stdout);

    init_gears();
    init_gear_lut();

    float angle = 0.0f;
    float speed = 0.05f;
    bool paused = false;
    bool running = true;

    uint32_t frame_count = 0;
    uint32_t fps = 60;
    time_t last_time = time(NULL);

    /* Initial full screen draw */
    fill_rect(0, 0, scr_w, scr_h, 0x0011141B);
    for (uint32_t gy = 40; gy < scr_h; gy += 40) fill_rect(0, gy, scr_w, 1, 0x00181D26);
    for (uint32_t gx = 40; gx < scr_w; gx += 40) fill_rect(gx, 0, 1, scr_h, 0x00181D26);
    fill_rect(0, 0, scr_w, 42, 0x00181D26);
    fill_rect(0, 42, scr_w, 2, 0x003DAEE9);
    char hud_title[80];
    snprintf(hud_title, sizeof(hud_title), "IotaOS Wayland-Gears Demo 2.0  |  Resolution: %ux%u", scr_w, scr_h);
    draw_string(20, 13, hud_title, 0x00FFFFFF, 0x00181D26);
    fill_rect(0, scr_h - 32, scr_w, 32, 0x00141820);
    fill_rect(0, scr_h - 33, scr_w, 1, 0x002B3342);
    static const char* pal_names[4] = {"Classic RGB", "Neon Cyberpunk", "KDE Breeze", "Bronze Gold"};
    char ctrl_str[128];
    snprintf(ctrl_str, sizeof(ctrl_str), "Controls: [+/-] Speed: %.2f  |  [Space] %s  |  [C] Theme: %s  |  [Q/ESC] Exit",
             speed, paused ? "Resume" : "Pause", pal_names[current_palette]);
    draw_string(20, scr_h - 22, ctrl_str, 0x00A0A8B6, 0x00141820);
    if (fb_direct) {
        memcpy(fb_direct, backbuffer, fb_total_size);
    } else if (fb_fd >= 0) {
        lseek(fb_fd, 0, SEEK_SET);
        write(fb_fd, backbuffer, fb_total_size);
    }

    int mid_x = scr_w / 2;
    int mid_y = scr_h / 2 - 20;
    int g_bx = mid_x - 245;
    int g_by = mid_y - 195;
    int g_bw = 360;
    int g_bh = 320;
    int prev_palette = current_palette;
    bool prev_paused = paused;
    float prev_speed = speed;

    while (running) {
        frame_count++;

        /* Non-blocking Key Input */
        struct pollfd pfd = { .fd = 0, .events = POLLIN };
        char ch;
        while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN) && read(0, &ch, 1) > 0) {
            if (ch == 27 || ch == 'q' || ch == 'Q') {
                running = false;
            } else if (ch == ' ') {
                paused = !paused;
            } else if (ch == '+' || ch == '=') {
                speed += 0.02f;
                if (speed > 0.30f) speed = 0.30f;
            } else if (ch == '-' || ch == '_') {
                speed -= 0.02f;
                if (speed < 0.01f) speed = 0.01f;
            } else if (ch == 'c' || ch == 'C') {
                current_palette = (current_palette + 1) % 4;
            }
        }

        if (!paused) {
            angle += speed;
            if (angle > 2.0f * M_PI * 100.0f) angle = 0.0f;
        }

        /* 1. Clear gears bounding box and redraw intersecting grid lines */
        fill_rect(g_bx, g_by, g_bw, g_bh, 0x0011141B);
        for (int gy = (g_by / 40) * 40; gy <= g_by + g_bh; gy += 40) {
            if (gy >= g_by && gy < g_by + g_bh) fill_rect(g_bx, gy, g_bw, 1, 0x00181D26);
        }
        for (int gx = (g_bx / 40) * 40; gx <= g_bx + g_bw; gx += 40) {
            if (gx >= g_bx && gx < g_bx + g_bw) fill_rect(gx, g_by, 1, g_bh, 0x00181D26);
        }

        /* 2. Render 3 Interlocking Gears */
        for (int i = 0; i < 3; i++) {
            float g_angle = (angle * gears[i].speed_mult) + gears[i].phase_offset;
            render_gear(&gears[i], g_angle);
        }

        /* 3. Top Banner & HUD (FPS Calculation) */
        time_t cur_time = time(NULL);
        if (cur_time != last_time) {
            fps = frame_count;
            frame_count = 0;
            last_time = cur_time;
        }

        char fps_str[64];
        float frame_ms = fps > 0 ? (1000.0f / (float)fps) : 0.0f;
        snprintf(fps_str, sizeof(fps_str), "[ %u FPS | %.1f ms ]", fps, frame_ms);
        fill_rect(scr_w - 204, 10, 194, 24, 0x00181D26);
        draw_string(scr_w - 200, 13, fps_str, 0x002ECC71, 0x00181D26);

        /* Update bottom legend if controls changed */
        bool legend_dirty = (current_palette != prev_palette || paused != prev_paused || speed != prev_speed);
        if (legend_dirty) {
            fill_rect(0, scr_h - 32, scr_w, 32, 0x00141820);
            snprintf(ctrl_str, sizeof(ctrl_str), "Controls: [+/-] Speed: %.2f  |  [Space] %s  |  [C] Theme: %s  |  [Q/ESC] Exit",
                     speed, paused ? "Resume" : "Pause", pal_names[current_palette]);
            draw_string(20, scr_h - 22, ctrl_str, 0x00A0A8B6, 0x00141820);
            prev_palette = current_palette;
            prev_paused = paused;
            prev_speed = speed;
        }

        /* 4. Flip dirty rects to screen */
        present_rect(g_bx, g_by, g_bw, g_bh);
        present_rect(scr_w - 204, 10, 194, 24);
        if (legend_dirty) {
            present_rect(0, scr_h - 33, scr_w, 33);
        }

        /* High refresh rate pacing (1ms) */
        usleep(1000);
    }

    /* Restore terminal */
    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);

    free(backbuffer);
    if (fb_fd >= 0) close(fb_fd);

    printf("Gears benchmark completed.\n");
    return 0;
}
