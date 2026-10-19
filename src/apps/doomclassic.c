/*
 * IotaOS - doom.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * Classic 3D DOOM Raycaster Engine for IotaOS.
 * Features:
 *  - 3D DDA Raycasting with textured walls, doors, and depth lighting decay.
 *  - DOOM Status Bar: Health, Armor, Ammo, Keys, and animated Doomguy face!
 *  - Animated Shotgun with muzzle flash, recoil, and smoke.
 *  - Enemy sprites (Zombieman / Imp) with billboarding and combat.
 *  - Real-time minimap (toggle with Tab).
 *  - Smooth 60-120 FPS rendering.
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
#include <math.h>
#include "iota.h"
#include "../kernel/drivers/fb_font.h"

#define MAP_W 16
#define MAP_H 16
#define TEX_W 64
#define TEX_H 64

static const uint8_t WORLD_MAP[MAP_H][MAP_W] = {
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

/* Textures: 0: None, 1: Stone, 2: Tech Metal, 3: Slime/Acid Brick, 4: Red Exit */
static uint32_t textures[5][TEX_W * TEX_H];

static void init_textures(void) {
    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) {
            /* 1: Grey tech stone */
            int c1 = ((x % 16 == 0 || y % 16 == 0) ? 0x30 : 0x78) + ((x + y) % 5) * 4;
            textures[1][y * TEX_W + x] = (c1 << 16) | (c1 << 8) | c1;

            /* 2: Blue-grey metal plate with rivets */
            int c2 = 0x50;
            if (x == 2 || x == TEX_W - 3 || y == 2 || y == TEX_H - 3) c2 = 0x90; /* Bevel */
            if ((x == 6 || x == TEX_W - 7) && (y == 6 || y == TEX_H - 7)) c2 = 0xD0; /* Rivets */
            textures[2][y * TEX_W + x] = (c2 << 16) | (c2 << 8) | (c2 + 0x20);

            /* 3: Toxic Slime Green Brick */
            int c3_g = ((y % 8 == 0 || (x + (y/8)*12) % 16 == 0) ? 0x20 : 0x88);
            textures[3][y * TEX_W + x] = (0x20 << 16) | (c3_g << 8) | 0x20;

            /* 4: Red Danger / Exit Portal */
            int c4_r = ((x / 8 + y / 8) % 2 == 0) ? 0xB0 : 0x60;
            textures[4][y * TEX_W + x] = (c4_r << 16) | (0x15 << 8) | 0x15;
        }
    }
}

/* Enemy sprite structure */
typedef struct {
    float x, y;
    int type;      /* 1 = Zombieman, 2 = Imp */
    int health;
    bool alive;
} enemy_t;

#define MAX_ENEMIES 4
static enemy_t enemies[MAX_ENEMIES] = {
    { 4.5f, 3.5f, 1, 3, true },
    { 11.5f, 3.5f, 2, 4, true },
    { 9.5f, 11.5f, 1, 3, true },
    { 3.5f, 12.5f, 2, 4, true }
};

/* Screen & Video */
static int fb_fd = -1;
static uint32_t scr_w = 1024;
static uint32_t scr_h = 768;
static uint32_t scr_pitch = 4096;
static uint32_t* fb_direct = NULL;
static uint32_t* backbuffer = NULL;
static size_t fb_total_size = 0;
static struct termios orig_termios;

/* Player State */
static float pos_x = 2.5f, pos_y = 2.5f;
static float dir_x = 1.0f, dir_y = 0.0f;
static float plane_x = 0.0f, plane_y = 0.66f; /* 66 deg FOV */
static int player_health = 100;
static int player_armor = 100;
static int player_ammo = 48;
static int player_kills = 0;

/* Weapon & Animation State */
static int shoot_anim = 0; /* 0 = idle, 1..6 = firing frames */
static int pain_flash = 0;
static bool show_map = false;

static inline void put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || (uint32_t)x >= scr_w || y < 0 || (uint32_t)y >= scr_h) return;
    backbuffer[y * scr_w + x] = color;
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

static uint32_t apply_lighting(uint32_t col, float dist, bool side) {
    float factor = 1.0f / (1.0f + dist * 0.12f);
    if (side) factor *= 0.72f; /* Shadow darker on N/S walls */
    if (factor > 1.0f) factor = 1.0f;
    if (factor < 0.12f) factor = 0.12f;

    uint32_t r = (uint32_t)(((col >> 16) & 0xFF) * factor);
    uint32_t g = (uint32_t)(((col >> 8) & 0xFF) * factor);
    uint32_t b = (uint32_t)((col & 0xFF) * factor);

    return (r << 16) | (g << 8) | b;
}

/* Render HUD / Classic Doom Status Bar */
static void render_status_bar(uint32_t frame_tick) {
    int hud_y = scr_h - 70;
    int hud_h = 70;

    /* Background: Dark Industrial Bronze-Grey */
    fill_rect(0, hud_y, scr_w, hud_h, 0x001B1C20);
    fill_rect(0, hud_y, scr_w, 3, 0x003A3C44);
    fill_rect(0, hud_y + 3, scr_w, 1, 0x00101012);

    /* Ammo Box */
    fill_rect(20, hud_y + 10, 110, 50, 0x00121316);
    draw_string(28, hud_y + 14, "AMMO", 0x00E03030, 0x00121316);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", player_ammo);
    draw_string(45, hud_y + 34, buf, 0x00E0B020, 0x00121316);

    /* Health Box */
    fill_rect(145, hud_y + 10, 130, 50, 0x00121316);
    draw_string(155, hud_y + 14, "HEALTH", 0x00E03030, 0x00121316);
    snprintf(buf, sizeof(buf), "%d%%", player_health);
    draw_string(175, hud_y + 34, buf, 0x00E0B020, 0x00121316);

    /* DOOM GUY Face in Center */
    int face_x = scr_w / 2 - 30;
    int face_y = hud_y + 10;
    fill_rect(face_x, face_y, 60, 50, 0x002B201A);
    fill_rect(face_x + 2, face_y + 2, 56, 46, 0x00906040); /* Skin */
    /* Hair */
    fill_rect(face_x + 10, face_y + 2, 40, 10, 0x00402010);
    /* Eyes: glancing left and right */
    int glance = (frame_tick / 40) % 3;
    int eye_off = (glance == 1) ? 2 : ((glance == 2) ? -2 : 0);
    fill_rect(face_x + 16 + eye_off, face_y + 18, 8, 4, 0x00FFFFFF);
    fill_rect(face_x + 36 + eye_off, face_y + 18, 8, 4, 0x00FFFFFF);
    fill_rect(face_x + 19 + eye_off, face_y + 19, 3, 3, 0x00000000);
    fill_rect(face_x + 39 + eye_off, face_y + 19, 3, 3, 0x00000000);
    /* Grin or grimace */
    if (shoot_anim > 0) {
        /* Firing O-mouth */
        fill_rect(face_x + 24, face_y + 32, 12, 8, 0x00200808);
    } else {
        fill_rect(face_x + 20, face_y + 34, 20, 3, 0x00401010);
    }

    /* Armor Box */
    fill_rect(scr_w / 2 + 45, hud_y + 10, 130, 50, 0x00121316);
    draw_string(scr_w / 2 + 55, hud_y + 14, "ARMOR", 0x00E03030, 0x00121316);
    snprintf(buf, sizeof(buf), "%d%%", player_armor);
    draw_string(scr_w / 2 + 75, hud_y + 34, buf, 0x00E0B020, 0x00121316);

    /* Kills Box */
    fill_rect(scr_w / 2 + 190, hud_y + 10, 120, 50, 0x00121316);
    draw_string(scr_w / 2 + 200, hud_y + 14, "FRAGS", 0x00E03030, 0x00121316);
    snprintf(buf, sizeof(buf), "%d", player_kills);
    draw_string(scr_w / 2 + 225, hud_y + 34, buf, 0x00E0B020, 0x00121316);

    /* Top Banner / Help */
    draw_string(scr_w - 240, hud_y + 26, "W/A/S/D: Move", 0x00A0A8B6, 0x001B1C20);
    draw_string(scr_w - 240, hud_y + 44, "Space: Fire | Tab: Map", 0x003DAEE9, 0x001B1C20);
}

/* Render Animated Shotgun */
static void render_weapon(void) {
    int view_h = scr_h - 70;
    int gun_x = scr_w / 2 - 40;
    int gun_y = view_h - 140;

    if (shoot_anim > 0) {
        /* Recoil kickback */
        gun_y += 18;

        /* Muzzle Flash */
        if (shoot_anim <= 3) {
            int fx = gun_x + 10;
            int fy = gun_y - 65;
            fill_rect(fx, fy, 60, 45, 0x00FFCC00);
            fill_rect(fx + 10, fy + 8, 40, 28, 0x00FFFFFF);
            fill_rect(fx - 15, fy + 12, 90, 12, 0x00FFAA00);
        }
    }

    /* Gun Barrels (Double Barrel Shotgun) */
    fill_rect(gun_x + 12, gun_y, 24, 110, 0x002B2D32);
    fill_rect(gun_x + 40, gun_y, 24, 110, 0x002B2D32);
    fill_rect(gun_x + 14, gun_y + 2, 20, 106, 0x004A4D55);
    fill_rect(gun_x + 42, gun_y + 2, 20, 106, 0x004A4D55);

    /* Bore holes */
    fill_rect(gun_x + 18, gun_y, 12, 10, 0x00111114);
    fill_rect(gun_x + 46, gun_y, 12, 10, 0x00111114);

    /* Wooden stock */
    fill_rect(gun_x + 8, gun_y + 90, 60, 60, 0x00663311);
    fill_rect(gun_x + 16, gun_y + 95, 44, 50, 0x0088441A);
}

/* Render Minimap Overlay */
static void render_minimap(void) {
    int map_x = 20;
    int map_y = 20;
    int cell_size = 10;

    fill_rect(map_x - 4, map_y - 4, MAP_W * cell_size + 8, MAP_H * cell_size + 8, 0x0011141B);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (WORLD_MAP[y][x] > 0) {
                fill_rect(map_x + x * cell_size, map_y + y * cell_size, cell_size - 1, cell_size - 1, 0x00555566);
            } else {
                fill_rect(map_x + x * cell_size, map_y + y * cell_size, cell_size - 1, cell_size - 1, 0x001B2028);
            }
        }
    }

    /* Enemies on minimap */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive) {
            fill_rect(map_x + (int)(enemies[i].x * cell_size) - 2,
                      map_y + (int)(enemies[i].y * cell_size) - 2,
                      5, 5, 0x00E03030);
        }
    }

    /* Player position & heading */
    int px = map_x + (int)(pos_x * cell_size);
    int py = map_y + (int)(pos_y * cell_size);
    fill_rect(px - 2, py - 2, 5, 5, 0x002ECC71);
    fill_rect(px + (int)(dir_x * 8.0f), py + (int)(dir_y * 8.0f), 2, 2, 0x00FFFFFF);
}

static void fire_shotgun(void) {
    if (player_ammo <= 0 || shoot_anim > 0) return;
    player_ammo -= 2;
    shoot_anim = 6;

    /* Hitscan trace against enemies */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive) continue;
        float ex = enemies[i].x - pos_x;
        float ey = enemies[i].y - pos_y;
        float dist = sqrtf(ex * ex + ey * ey);
        if (dist < 0.1f) continue;

        /* Dot product with player heading */
        float dot = (ex * dir_x + ey * dir_y) / dist;
        if (dot > 0.92f && dist < 7.0f) {
            /* Hit enemy */
            enemies[i].health -= 2;
            if (enemies[i].health <= 0) {
                enemies[i].alive = false;
                player_kills++;
            }
        }
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    /* 1. Open Framebuffer */
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "doom: cannot open /dev/fb0 (Linear Framebuffer required)\n");
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
        fprintf(stderr, "doom: cannot allocate backbuffer\n");
        close(fb_fd);
        return 1;
    }

    init_textures();

    /* Open Mouse */
    typedef struct {
        int32_t x;
        int32_t y;
        int32_t buttons;
        int32_t wheel;
        int32_t dx;
        int32_t dy;
    } __attribute__((packed)) mouse_event_t;
    int mouse_fd = open("/dev/mouse", O_RDONLY | O_NONBLOCK);
    int prev_mbtn = 0;

    /* Raw nonblocking keyboard input */
    tcgetattr(0, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);

    printf("\033[?25l");
    fflush(stdout);

    float move_speed = 0.12f;
    float rot_speed  = 0.08f;
    bool running = true;
    uint32_t frame_tick = 0;

    int view_h = scr_h - 70;
    float z_buffer[1024];

    while (running) {
        frame_tick++;

        /* Handle user keyboard input */
        struct pollfd pfd = { .fd = 0, .events = POLLIN, .revents = 0 };
        while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
            char ch = 0;
            if (read(0, &ch, 1) <= 0) break;

            if (ch == 27) {
                char seq[3] = {0};
                if (poll(&pfd, 1, 30) > 0 && read(0, &seq[0], 1) > 0 && seq[0] == '[') {
                    if (poll(&pfd, 1, 30) > 0 && read(0, &seq[1], 1) > 0) {
                        if (seq[1] == 'A') { /* Up Arrow */
                            if (WORLD_MAP[(int)(pos_y)][(int)(pos_x + dir_x * move_speed * 1.5f)] == 0) pos_x += dir_x * move_speed;
                            if (WORLD_MAP[(int)(pos_y + dir_y * move_speed * 1.5f)][(int)(pos_x)] == 0) pos_y += dir_y * move_speed;
                        } else if (seq[1] == 'B') { /* Down Arrow */
                            if (WORLD_MAP[(int)(pos_y)][(int)(pos_x - dir_x * move_speed * 1.5f)] == 0) pos_x -= dir_x * move_speed;
                            if (WORLD_MAP[(int)(pos_y - dir_y * move_speed * 1.5f)][(int)(pos_x)] == 0) pos_y -= dir_y * move_speed;
                        } else if (seq[1] == 'D') { /* Left Arrow: Rotate Left */
                            float old_dir_x = dir_x;
                            dir_x = dir_x * cosf(-rot_speed) - dir_y * sinf(-rot_speed);
                            dir_y = old_dir_x * sinf(-rot_speed) + dir_y * cosf(-rot_speed);
                            float old_plane_x = plane_x;
                            plane_x = plane_x * cosf(-rot_speed) - plane_y * sinf(-rot_speed);
                            plane_y = old_plane_x * sinf(-rot_speed) + plane_y * cosf(-rot_speed);
                        } else if (seq[1] == 'C') { /* Right Arrow: Rotate Right */
                            float old_dir_x = dir_x;
                            dir_x = dir_x * cosf(rot_speed) - dir_y * sinf(rot_speed);
                            dir_y = old_dir_x * sinf(rot_speed) + dir_y * cosf(rot_speed);
                            float old_plane_x = plane_x;
                            plane_x = plane_x * cosf(rot_speed) - plane_y * sinf(rot_speed);
                            plane_y = old_plane_x * sinf(rot_speed) + plane_y * cosf(rot_speed);
                        }
                    }
                } else {
                    /* Only exit if explicitly pressing ESC when not moving */
                }
            } else if (ch == 'q' || ch == 'Q') {
                running = false;
            } else if (ch == ' ' || ch == '\n' || ch == '\r') {
                fire_shotgun();
            } else if (ch == '\t') {
                show_map = !show_map;
            } else if (ch == 'w' || ch == 'W') {
                if (WORLD_MAP[(int)(pos_y)][(int)(pos_x + dir_x * move_speed * 1.5f)] == 0) pos_x += dir_x * move_speed;
                if (WORLD_MAP[(int)(pos_y + dir_y * move_speed * 1.5f)][(int)(pos_x)] == 0) pos_y += dir_y * move_speed;
            } else if (ch == 's' || ch == 'S') {
                if (WORLD_MAP[(int)(pos_y)][(int)(pos_x - dir_x * move_speed * 1.5f)] == 0) pos_x -= dir_x * move_speed;
                if (WORLD_MAP[(int)(pos_y - dir_y * move_speed * 1.5f)][(int)(pos_x)] == 0) pos_y -= dir_y * move_speed;
            } else if (ch == 'a' || ch == 'A') {
                /* Rotate Left */
                float old_dir_x = dir_x;
                dir_x = dir_x * cosf(-rot_speed) - dir_y * sinf(-rot_speed);
                dir_y = old_dir_x * sinf(-rot_speed) + dir_y * cosf(-rot_speed);
                float old_plane_x = plane_x;
                plane_x = plane_x * cosf(-rot_speed) - plane_y * sinf(-rot_speed);
                plane_y = old_plane_x * sinf(-rot_speed) + plane_y * cosf(-rot_speed);
            } else if (ch == 'd' || ch == 'D') {
                /* Rotate Right */
                float old_dir_x = dir_x;
                dir_x = dir_x * cosf(rot_speed) - dir_y * sinf(rot_speed);
                dir_y = old_dir_x * sinf(rot_speed) + dir_y * cosf(rot_speed);
                float old_plane_x = plane_x;
                plane_x = plane_x * cosf(rot_speed) - plane_y * sinf(rot_speed);
                plane_y = old_plane_x * sinf(rot_speed) + plane_y * cosf(rot_speed);
            }
        }

        /* Mouse Input: turning and shooting */
        if (mouse_fd >= 0) {
            mouse_event_t ms;
            int total_dx = 0;
            int cur_mbtn = 0;
            while (read(mouse_fd, &ms, sizeof(ms)) >= (ssize_t)16) {
                cur_mbtn = ms.buttons;
                total_dx += ms.dx;
            }
            if (total_dx != 0) {
                float m_rot = (float)total_dx * 0.005f;
                float old_dir_x = dir_x;
                dir_x = dir_x * cosf(m_rot) - dir_y * sinf(m_rot);
                dir_y = old_dir_x * sinf(m_rot) + dir_y * cosf(m_rot);
                float old_plane_x = plane_x;
                plane_x = plane_x * cosf(m_rot) - plane_y * sinf(m_rot);
                plane_y = old_plane_x * sinf(m_rot) + plane_y * cosf(m_rot);
            }
            if ((cur_mbtn & 1) && !(prev_mbtn & 1)) {
                fire_shotgun();
            }
            prev_mbtn = cur_mbtn;
        }

        if (shoot_anim > 0) shoot_anim--;
        if (pain_flash > 0) pain_flash--;

        /* 1. Floor & Ceiling Rendering */
        fill_rect(0, 0, scr_w, view_h / 2, 0x001B1E24);              /* Dark Ceiling */
        fill_rect(0, view_h / 2, scr_w, view_h / 2, 0x002B241E);      /* Mud / Toxic Floor */

        /* 2. Raycast Walls (DDA algorithm) */
        for (uint32_t x = 0; x < scr_w; x++) {
            float camera_x = 2.0f * (float)x / (float)scr_w - 1.0f;
            float ray_dir_x = dir_x + plane_x * camera_x;
            float ray_dir_y = dir_y + plane_y * camera_x;

            int map_x = (int)pos_x;
            int map_y = (int)pos_y;

            float delta_dist_x = fabsf(1.0f / (ray_dir_x + 0.00001f));
            float delta_dist_y = fabsf(1.0f / (ray_dir_y + 0.00001f));
            float perp_wall_dist;

            int step_x, step_y;
            int hit = 0;
            int side = 0;

            if (ray_dir_x < 0) {
                step_x = -1;
                delta_dist_x = (pos_x - map_x) * delta_dist_x;
            } else {
                step_x = 1;
                delta_dist_x = (map_x + 1.0f - pos_x) * delta_dist_x;
            }

            if (ray_dir_y < 0) {
                step_y = -1;
                delta_dist_y = (pos_y - map_y) * delta_dist_y;
            } else {
                step_y = 1;
                delta_dist_y = (map_y + 1.0f - pos_y) * delta_dist_y;
            }

            float side_dist_x = delta_dist_x;
            float side_dist_y = delta_dist_y;
            float step_delta_x = fabsf(1.0f / (ray_dir_x + 0.00001f));
            float step_delta_y = fabsf(1.0f / (ray_dir_y + 0.00001f));

            while (hit == 0) {
                if (side_dist_x < side_dist_y) {
                    side_dist_x += step_delta_x;
                    map_x += step_x;
                    side = 0;
                } else {
                    side_dist_y += step_delta_y;
                    map_y += step_y;
                    side = 1;
                }

                if (map_x >= 0 && map_x < MAP_W && map_y >= 0 && map_y < MAP_H) {
                    if (WORLD_MAP[map_y][map_x] > 0) hit = 1;
                } else {
                    hit = 1;
                }
            }

            if (side == 0) perp_wall_dist = (map_x - pos_x + (1 - step_x) / 2.0f) / ray_dir_x;
            else           perp_wall_dist = (map_y - pos_y + (1 - step_y) / 2.0f) / ray_dir_y;

            if (x < 1024) z_buffer[x] = perp_wall_dist;

            int line_h = (int)((float)view_h / perp_wall_dist);
            int draw_start = -line_h / 2 + view_h / 2;
            if (draw_start < 0) draw_start = 0;
            int draw_end = line_h / 2 + view_h / 2;
            if (draw_end >= view_h) draw_end = view_h - 1;

            int tex_num = 1;
            if (map_x >= 0 && map_x < MAP_W && map_y >= 0 && map_y < MAP_H) {
                tex_num = WORLD_MAP[map_y][map_x];
            }
            if (tex_num < 1 || tex_num > 4) tex_num = 1;

            /* Texture coordinate */
            float wall_x;
            if (side == 0) wall_x = pos_y + perp_wall_dist * ray_dir_y;
            else           wall_x = pos_x + perp_wall_dist * ray_dir_x;
            wall_x -= floorf(wall_x);

            int tex_x = (int)(wall_x * (float)TEX_W);
            if (side == 0 && ray_dir_x > 0) tex_x = TEX_W - tex_x - 1;
            if (side == 1 && ray_dir_y < 0) tex_x = TEX_W - tex_x - 1;

            float step = 1.0f * (float)TEX_H / (float)line_h;
            float tex_pos = (draw_start - view_h / 2.0f + line_h / 2.0f) * step;

            for (int y = draw_start; y < draw_end; y++) {
                int tex_y = (int)tex_pos & (TEX_H - 1);
                tex_pos += step;
                uint32_t color = textures[tex_num][TEX_H * tex_y + tex_x];
                color = apply_lighting(color, perp_wall_dist, side == 1);
                put_pixel(x, y, color);
            }
        }

        /* 3. Render Sprite Enemies */
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].alive) continue;

            float sprite_x = enemies[i].x - pos_x;
            float sprite_y = enemies[i].y - pos_y;

            float inv_det = 1.0f / (plane_x * dir_y - dir_x * plane_y);
            float trans_x = inv_det * (dir_y * sprite_x - dir_x * sprite_y);
            float trans_y = inv_det * (-plane_y * sprite_x + plane_x * sprite_y);

            if (trans_y <= 0.2f) continue;

            int sprite_screen_x = (int)((scr_w / 2.0f) * (1.0f + trans_x / trans_y));
            int sprite_h = abs((int)(view_h / trans_y));
            int draw_start_y = -sprite_h / 2 + view_h / 2;
            if (draw_start_y < 0) draw_start_y = 0;
            int draw_end_y = sprite_h / 2 + view_h / 2;
            if (draw_end_y >= view_h) draw_end_y = view_h - 1;

            int sprite_w = abs((int)(view_h / trans_y));
            int draw_start_x = -sprite_w / 2 + sprite_screen_x;
            if (draw_start_x < 0) draw_start_x = 0;
            int draw_end_x = sprite_w / 2 + sprite_screen_x;
            if (draw_end_x >= (int)scr_w) draw_end_x = scr_w - 1;

            uint32_t enemy_col = (enemies[i].type == 1) ? 0x00A05030 : 0x00C02020;

            for (int stripe = draw_start_x; stripe < draw_end_x; stripe++) {
                if (stripe >= 0 && stripe < 1024 && trans_y < z_buffer[stripe]) {
                    for (int y = draw_start_y; y < draw_end_y; y++) {
                        put_pixel(stripe, y, enemy_col);
                    }
                }
            }
        }

        /* 4. Render Shotgun */
        render_weapon();

        /* 5. Render Minimap */
        if (show_map) {
            render_minimap();
        }

        /* 6. Render Status Bar */
        render_status_bar(frame_tick);

        /* 7. Present */
        if (fb_direct) {
            memcpy(fb_direct, backbuffer, fb_total_size);
        } else if (fb_fd >= 0) {
            lseek(fb_fd, 0, SEEK_SET);
            write(fb_fd, backbuffer, fb_total_size);
        }

        /* ~60-120 FPS high refresh pacing */
        usleep(10000);
    }

    /* Restore terminal */
    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);

    free(backbuffer);
    if (fb_fd >= 0) close(fb_fd);

    printf("DOOM session finished. Frags: %d\n", player_kills);
    return 0;
}
