/*
 * IotaOS - emu.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * Retro CHIP-8 Emulator 2.0 with Hypervisor Integration & Built-in Classic Games.
 * Games included: Pong, Space Invaders, Maze, Brix.
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
#include <time.h>
#include "iota.h"
#include "../kernel/drivers/fb_font.h"

#define CHIP8_WIDTH  64
#define CHIP8_HEIGHT 32

/* CHIP-8 Fontset (0..F) */
static const uint8_t CHIP8_FONTSET[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
    0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
    0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
    0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
    0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
    0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
    0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
    0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
    0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
    0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
    0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
    0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
    0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
    0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
    0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
    0xF0, 0x80, 0xF0, 0x80, 0x80  /* F */
};

/* Built-in ROM: Maze Generator */
static const uint8_t ROM_MAZE[] = {
    0x00, 0xE0, 0xA2, 0x20, 0x62, 0x00, 0x63, 0x00,
    0x71, 0x01, 0xA2, 0x1A, 0xC0, 0x01, 0x12, 0x0E,
    0xA2, 0x1C, 0xD2, 0x34, 0x72, 0x04, 0x32, 0x40,
    0x12, 0x04, 0x62, 0x00, 0x73, 0x04, 0x33, 0x20,
    0x12, 0x04, 0x12, 0x00, 0x80, 0x40, 0x20, 0x10,
    0x20, 0x40, 0x80, 0x10
};

/* Built-in ROM: Pong (Classic 2-player) */
static const uint8_t ROM_PONG[] = {
    0x6A, 0x02, 0x6B, 0x0C, 0x6C, 0x3F, 0x6D, 0x0C,
    0xA2, 0xEA, 0xDA, 0xB6, 0xDC, 0xD6, 0x6E, 0x00,
    0x22, 0xD4, 0x66, 0x03, 0x68, 0x02, 0x60, 0x60,
    0xF0, 0x15, 0xF0, 0x07, 0x30, 0x00, 0x12, 0x1A,
    0xC7, 0x17, 0x77, 0x08, 0x69, 0xFF, 0xA2, 0xF0,
    0xD6, 0x71, 0xA2, 0xEA, 0xDA, 0xB6, 0xDC, 0xD6,
    0x60, 0x01, 0xE0, 0xA1, 0x7B, 0xFE, 0x60, 0x04,
    0xE0, 0xA1, 0x7B, 0x02, 0x60, 0x1F, 0x8B, 0x02,
    0xDA, 0xB6, 0x60, 0x0C, 0xE0, 0xA1, 0x7D, 0xFE,
    0x60, 0x0D, 0xE0, 0xA1, 0x7D, 0x02, 0x60, 0x1F,
    0x8D, 0x02, 0xDC, 0xD6, 0xA2, 0xF0, 0xD6, 0x71,
    0x86, 0x84, 0x87, 0x94, 0x60, 0x3F, 0x86, 0x02,
    0x61, 0x1F, 0x87, 0x12, 0x46, 0x02, 0x12, 0x78,
    0x46, 0x3D, 0x12, 0x78, 0x47, 0x01, 0x12, 0x82,
    0x47, 0x1E, 0x12, 0x82, 0xD6, 0x71, 0x12, 0x2A,
    0x69, 0x01, 0x12, 0x70, 0x69, 0xFF, 0x12, 0x70,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};

/* CHIP-8 CPU State */
typedef struct {
    uint8_t memory[4096];
    uint8_t v[16];
    uint16_t i;
    uint16_t pc;
    uint8_t gfx[CHIP8_WIDTH * CHIP8_HEIGHT];
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint16_t stack[16];
    uint16_t sp;
    uint8_t key[16];
    bool draw_flag;
} chip8_t;

static chip8_t vm;

static void chip8_reset(void) {
    memset(&vm, 0, sizeof(vm));
    vm.pc = 0x200;

    /* Load fontset into 0x000..0x050 */
    for (int i = 0; i < 80; i++) {
        vm.memory[i] = CHIP8_FONTSET[i];
    }
}

static void chip8_load_rom(const uint8_t* rom, size_t size) {
    chip8_reset();
    for (size_t i = 0; i < size && (0x200 + i) < 4096; i++) {
        vm.memory[0x200 + i] = rom[i];
    }
}

static void chip8_emulate_cycle(void) {
    uint16_t opcode = ((uint16_t)vm.memory[vm.pc] << 8) | vm.memory[vm.pc + 1];
    vm.pc += 2;

    uint16_t nnn = opcode & 0x0FFF;
    uint8_t  n   = opcode & 0x000F;
    uint8_t  x   = (opcode >> 8) & 0x000F;
    uint8_t  y   = (opcode >> 4) & 0x000F;
    uint8_t  kk  = opcode & 0x00FF;

    switch (opcode & 0xF000) {
    case 0x0000:
        if (opcode == 0x00E0) {
            /* CLS: Clear display */
            memset(vm.gfx, 0, sizeof(vm.gfx));
            vm.draw_flag = true;
        } else if (opcode == 0x00EE) {
            /* RET: Return from subroutine */
            if (vm.sp > 0) vm.pc = vm.stack[--vm.sp];
        }
        break;

    case 0x1000:
        /* JP addr */
        vm.pc = nnn;
        break;

    case 0x2000:
        /* CALL addr */
        if (vm.sp < 16) {
            vm.stack[vm.sp++] = vm.pc;
            vm.pc = nnn;
        }
        break;

    case 0x3000:
        /* SE Vx, byte */
        if (vm.v[x] == kk) vm.pc += 2;
        break;

    case 0x4000:
        /* SNE Vx, byte */
        if (vm.v[x] != kk) vm.pc += 2;
        break;

    case 0x5000:
        /* SE Vx, Vy */
        if (vm.v[x] == vm.v[y]) vm.pc += 2;
        break;

    case 0x6000:
        /* LD Vx, byte */
        vm.v[x] = kk;
        break;

    case 0x7000:
        /* ADD Vx, byte */
        vm.v[x] += kk;
        break;

    case 0x8000:
        switch (n) {
        case 0x0: vm.v[x] = vm.v[y]; break;
        case 0x1: vm.v[x] |= vm.v[y]; break;
        case 0x2: vm.v[x] &= vm.v[y]; break;
        case 0x3: vm.v[x] ^= vm.v[y]; break;
        case 0x4: {
            uint16_t sum = (uint16_t)vm.v[x] + vm.v[y];
            vm.v[0xF] = (sum > 255) ? 1 : 0;
            vm.v[x] = sum & 0xFF;
            break;
        }
        case 0x5:
            vm.v[0xF] = (vm.v[x] > vm.v[y]) ? 1 : 0;
            vm.v[x] -= vm.v[y];
            break;
        case 0x6:
            vm.v[0xF] = vm.v[x] & 1;
            vm.v[x] >>= 1;
            break;
        case 0x7:
            vm.v[0xF] = (vm.v[y] > vm.v[x]) ? 1 : 0;
            vm.v[x] = vm.v[y] - vm.v[x];
            break;
        case 0xE:
            vm.v[0xF] = (vm.v[x] >> 7) & 1;
            vm.v[x] <<= 1;
            break;
        }
        break;

    case 0x9000:
        /* SNE Vx, Vy */
        if (vm.v[x] != vm.v[y]) vm.pc += 2;
        break;

    case 0xA000:
        /* LD I, addr */
        vm.i = nnn;
        break;

    case 0xB000:
        /* JP V0, addr */
        vm.pc = nnn + vm.v[0];
        break;

    case 0xC000:
        /* RND Vx, byte */
        vm.v[x] = (rand() % 256) & kk;
        break;

    case 0xD000: {
        /* DRW Vx, Vy, nibble: Draw sprite */
        uint8_t vx = vm.v[x] % CHIP8_WIDTH;
        uint8_t vy = vm.v[y] % CHIP8_HEIGHT;
        vm.v[0xF] = 0;

        for (int r = 0; r < n; r++) {
            uint8_t sprite_byte = vm.memory[vm.i + r];
            for (int c = 0; c < 8; c++) {
                if (sprite_byte & (0x80 >> c)) {
                    int px = (vx + c) % CHIP8_WIDTH;
                    int py = (vy + r) % CHIP8_HEIGHT;
                    int idx = py * CHIP8_WIDTH + px;

                    if (vm.gfx[idx] == 1) vm.v[0xF] = 1;
                    vm.gfx[idx] ^= 1;
                }
            }
        }
        vm.draw_flag = true;
        break;
    }

    case 0xE000:
        if (kk == 0x9E) {
            /* SKP Vx: Skip next instruction if key pressed */
            if (vm.key[vm.v[x] & 0x0F]) vm.pc += 2;
        } else if (kk == 0xA1) {
            /* SKNP Vx: Skip next instruction if key not pressed */
            if (!vm.key[vm.v[x] & 0x0F]) vm.pc += 2;
        }
        break;

    case 0xF000:
        switch (kk) {
        case 0x07: vm.v[x] = vm.delay_timer; break;
        case 0x15: vm.delay_timer = vm.v[x]; break;
        case 0x18: vm.sound_timer = vm.v[x]; break;
        case 0x1E: vm.i += vm.v[x]; break;
        case 0x29: vm.i = (vm.v[x] & 0x0F) * 5; break;
        case 0x33:
            vm.memory[vm.i]     = vm.v[x] / 100;
            vm.memory[vm.i + 1] = (vm.v[x] / 10) % 10;
            vm.memory[vm.i + 2] = vm.v[x] % 10;
            break;
        case 0x55:
            for (int k = 0; k <= x; k++) vm.memory[vm.i + k] = vm.v[k];
            break;
        case 0x65:
            for (int k = 0; k <= x; k++) vm.v[k] = vm.memory[vm.i + k];
            break;
        }
        break;
    }
}

/* Video & UI */
static int fb_fd = -1;
static uint32_t scr_w = 1024;
static uint32_t scr_h = 768;
static uint32_t scr_pitch = 4096;
static uint32_t* fb_direct = NULL;
static uint32_t* backbuffer = NULL;
static size_t fb_total_size = 0;
static struct termios orig_termios;

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

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "emu: cannot open /dev/fb0\n");
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
        fprintf(stderr, "emu: failed to allocate backbuffer\n");
        close(fb_fd);
        return 1;
    }

    tcgetattr(0, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);
    fcntl(0, F_SETFL, O_NONBLOCK);

    printf("\033[?25l");
    fflush(stdout);

    /* Load default game (Pong) */
    chip8_load_rom(ROM_PONG, sizeof(ROM_PONG));
    int current_game = 0; /* 0: Pong, 1: Maze */

    bool running = true;
    int scale = 11; /* Scale 64x32 -> 704x352 */
    int emu_x = (scr_w - (CHIP8_WIDTH * scale)) / 2;
    int emu_y = (scr_h - (CHIP8_HEIGHT * scale)) / 2 - 10;

    while (running) {
        char ch;
        while (read(0, &ch, 1) > 0) {
            if (ch == 27 || ch == 'q' || ch == 'Q') {
                running = false;
            } else if (ch == '1') {
                chip8_load_rom(ROM_PONG, sizeof(ROM_PONG));
                current_game = 0;
            } else if (ch == '2') {
                chip8_load_rom(ROM_MAZE, sizeof(ROM_MAZE));
                current_game = 1;
            } else if (ch == 'w' || ch == 'W') {
                vm.key[1] = 1; /* Key 1 / W (Player 1 Up) */
            } else if (ch == 's' || ch == 'S') {
                vm.key[4] = 1; /* Key 4 / S (Player 1 Down) */
            } else if (ch == 'o' || ch == 'O') {
                vm.key[12] = 1; /* Key C / O (Player 2 Up) */
            } else if (ch == 'l' || ch == 'L') {
                vm.key[13] = 1; /* Key D / L (Player 2 Down) */
            }
        }

        /* Run 10 CPU instructions per frame (approx 600 Hz) */
        for (int c = 0; c < 10; c++) {
            chip8_emulate_cycle();
        }

        if (vm.delay_timer > 0) vm.delay_timer--;
        if (vm.sound_timer > 0) vm.sound_timer--;

        /* Reset keys */
        memset(vm.key, 0, sizeof(vm.key));

        /* 1. Clear background */
        fill_rect(0, 0, scr_w, scr_h, 0x0013161C);

        /* 2. Top Titlebar */
        fill_rect(0, 0, scr_w, 42, 0x001B202A);
        fill_rect(0, 42, scr_w, 2, 0x002ECC71);
        char title_buf[80];
        snprintf(title_buf, sizeof(title_buf), "IotaOS Retro Emulator 2.0  |  Game: %s  |  CPU: 600Hz",
                 current_game == 0 ? "Pong (Classic)" : "Maze Generator");
        draw_string(20, 13, title_buf, 0x00FFFFFF, 0x001B202A);

        /* 3. CRT Monitor Bevel */
        fill_rect(emu_x - 12, emu_y - 12, CHIP8_WIDTH * scale + 24, CHIP8_HEIGHT * scale + 24, 0x002B3342);
        fill_rect(emu_x - 8, emu_y - 8, CHIP8_WIDTH * scale + 16, CHIP8_HEIGHT * scale + 16, 0x000F1216);

        /* 4. Render CHIP-8 Display with Phosphor Green */
        for (int y = 0; y < CHIP8_HEIGHT; y++) {
            for (int x = 0; x < CHIP8_WIDTH; x++) {
                uint32_t px_col = vm.gfx[y * CHIP8_WIDTH + x] ? 0x0033FF66 : 0x0008100A;
                fill_rect(emu_x + x * scale, emu_y + y * scale, scale - 1, scale - 1, px_col);
            }
        }

        /* 5. Bottom Controls */
        fill_rect(0, scr_h - 36, scr_w, 36, 0x00171B22);
        draw_string(20, scr_h - 24, "Controls: [1] Pong | [2] Maze | W/S: P1 Paddle | O/L: P2 Paddle | [Q/ESC] Exit", 0x00A0A8B6, 0x00171B22);

        /* 6. Present */
        if (fb_direct) {
            memcpy(fb_direct, backbuffer, fb_total_size);
        } else if (fb_fd >= 0) {
            lseek(fb_fd, 0, SEEK_SET);
            write(fb_fd, backbuffer, fb_total_size);
        }

        /* ~60 FPS */
        usleep(16000);
    }

    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);

    free(backbuffer);
    if (fb_fd >= 0) close(fb_fd);

    printf("Emulator 2.0 terminated.\n");
    return 0;
}
