/*
 * IotaOS - doomgeneric_iota.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * Native IotaOS Framebuffer & Input backend for authentic id Software DOOM.
 */

#include "doomkeys.h"
#include "d_event.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <poll.h>

static int fb_fd = -1;
static uint32_t* fb_direct = NULL;
static uint32_t scr_w = 1024;
static uint32_t scr_h = 768;
static uint32_t scr_pitch = 4096;

static int mouse_fd = -1;
static int cur_mbtn = 0;
static int prev_mbtn = 0;

/* Mouse event structure matching IotaOS kernel */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t buttons;
    int32_t wheel;
    int32_t dx;
    int32_t dy;
} __attribute__((packed)) mouse_event_t;

/* Viewport positioning */
static int view_x = 0;
static int view_y = 0;

/* Key queue for Doom event polling */
#define KEYQUEUE_SIZE 128
static unsigned short key_queue[KEYQUEUE_SIZE];
static unsigned int key_queue_head = 0;
static unsigned int key_queue_tail = 0;

#define MAX_TRACKED_KEYS 256
static uint32_t key_held_until[MAX_TRACKED_KEYS] = {0};

static struct termios orig_termios;

static void queue_key(int pressed, unsigned char key) {
    if (key == 0) return;
    unsigned int next_head = (key_queue_head + 1) % KEYQUEUE_SIZE;
    if (next_head != key_queue_tail) {
        key_queue[key_queue_head] = ((unsigned short)(pressed ? 1 : 0) << 8) | key;
        key_queue_head = next_head;
    }
}

static void press_key_decay(unsigned char key, uint32_t duration_ms) {
    if (key == 0) return;
    if (key_held_until[key] == 0) {
        queue_key(1, key);
    }
    uint32_t expire = DG_GetTicksMs() + duration_ms;
    if (expire > key_held_until[key]) {
        key_held_until[key] = expire;
    }
}

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
    fflush(stdout);
}

void DG_Init(void) {
    /* Point DOOMWADDIR at an existing doom1.wad so the game is found no
       matter which cwd it is launched from (shell, desktop icon, ...). */
    if (!getenv("DOOMWADDIR")) {
        static const char *dirs[] = {"/home/iota", "/root", "/usr/share/games/doom", "/usr/share", "/"};
        for (unsigned d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
            char probe[128];
            snprintf(probe, sizeof(probe), "%s/doom1.wad", dirs[d]);
            int fd = open(probe, O_RDONLY);
            if (fd >= 0) {
                close(fd);
                setenv("DOOMWADDIR", dirs[d], 1);
                break;
            }
        }
    }

    /* Open Framebuffer */
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd >= 0) {
        uint32_t fb_meta[6] = {0};
        if (ioctl(fb_fd, 0x4600, fb_meta) == 0 && fb_meta[0] > 0 && fb_meta[1] > 0) {
            scr_w = fb_meta[0];
            scr_h = fb_meta[1];
            scr_pitch = fb_meta[2];
            fb_direct = (uint32_t*)fb_meta[4];
            if (fb_direct) {
                memset(fb_direct, 0, scr_pitch * scr_h);
            }
        }
    }

    /* Center 640x400 Doom viewport on display */
    if (scr_w >= DOOMGENERIC_RESX) {
        view_x = (scr_w - DOOMGENERIC_RESX) / 2;
    } else {
        view_x = 0;
    }
    if (scr_h >= DOOMGENERIC_RESY) {
        view_y = (scr_h - DOOMGENERIC_RESY) / 2;
    } else {
        view_y = 0;
    }

    /* Draw dark Slate letterbox border if framebuffer is larger than viewport */
    if (fb_direct) {
        for (uint32_t y = 0; y < scr_h; y++) {
            for (uint32_t x = 0; x < scr_w; x++) {
                if (x < (uint32_t)view_x || x >= (uint32_t)(view_x + DOOMGENERIC_RESX) ||
                    y < (uint32_t)view_y || y >= (uint32_t)(view_y + DOOMGENERIC_RESY)) {
                    fb_direct[y * (scr_pitch / 4) + x] = 0x0015181F; /* Dark Nord slate */
                }
            }
        }
    }

    /* Open Mouse */
    mouse_fd = open("/dev/mouse", O_RDONLY | O_NONBLOCK);

    /* Configure Terminal for Raw Input */
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(restore_terminal);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    /* Hide text console cursor */
    printf("\033[?25l");
    fflush(stdout);
}

static void poll_inputs(void) {
    /* Process key release timers */
    uint32_t now = DG_GetTicksMs();
    for (int k = 0; k < MAX_TRACKED_KEYS; k++) {
        if (key_held_until[k] > 0 && now >= key_held_until[k]) {
            queue_key(0, (unsigned char)k);
            key_held_until[k] = 0;
        }
    }

    /* 1. Poll Terminal Keypresses */
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN, .revents = 0 };
    while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) <= 0) break;

        if (ch == 27) { /* ESC or ANSI sequence */
            char seq[3] = {0};
            if (poll(&pfd, 1, 30) > 0 && read(STDIN_FILENO, &seq[0], 1) > 0 && seq[0] == '[') {
                if (poll(&pfd, 1, 30) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                    if (seq[1] == 'A') { /* Up arrow: Forward */
                        press_key_decay(KEY_UPARROW, 180);
                    } else if (seq[1] == 'B') { /* Down arrow: Backward */
                        press_key_decay(KEY_DOWNARROW, 180);
                    } else if (seq[1] == 'D') { /* Left arrow: Turn Left */
                        press_key_decay(KEY_LEFTARROW, 180);
                    } else if (seq[1] == 'C') { /* Right arrow: Turn Right */
                        press_key_decay(KEY_RIGHTARROW, 180);
                    }
                }
            } else {
                /* Pure ESC key */
                press_key_decay(KEY_ESCAPE, 80);
            }
        } else if (ch == '\n' || ch == '\r') {
            press_key_decay(KEY_ENTER, 80);
        } else if (ch == ' ' || ch == 'e' || ch == 'E') {
            press_key_decay(KEY_USE, 150);
        } else if (ch == '\t') {
            press_key_decay(KEY_TAB, 100);
        } else if (ch == 'w' || ch == 'W') {
            press_key_decay(KEY_UPARROW, 180);
        } else if (ch == 's' || ch == 'S') {
            press_key_decay(KEY_DOWNARROW, 180);
        } else if (ch == 'a' || ch == 'A') {
            /* Turn Left (WASD) */
            press_key_decay(KEY_LEFTARROW, 180);
        } else if (ch == 'd' || ch == 'D') {
            /* Turn Right (WASD) */
            press_key_decay(KEY_RIGHTARROW, 180);
        } else if (ch == ',' || ch == '<' || ch == 'z' || ch == 'Z') {
            /* Strafe Left */
            press_key_decay(KEY_STRAFE_L, 180);
        } else if (ch == '.' || ch == '>' || ch == 'c' || ch == 'C') {
            /* Strafe Right */
            press_key_decay(KEY_STRAFE_R, 180);
        } else if (ch == 'q' || ch == 'Q' || ch == 'f' || ch == 'F') {
            press_key_decay(KEY_FIRE, 150);
        } else if (ch >= '1' && ch <= '7') {
            press_key_decay((unsigned char)ch, 100);
        } else if (ch == 'y' || ch == 'Y' || ch == 'n' || ch == 'N') {
            press_key_decay((unsigned char)ch, 100);
        }
    }

    /* 2. Poll Mouse Input */
    if (mouse_fd >= 0) {
        mouse_event_t ms;
        int total_dx = 0;
        int total_dy = 0;
        while (read(mouse_fd, &ms, sizeof(ms)) >= (ssize_t)16) {
            cur_mbtn = ms.buttons;
            total_dx += ms.dx;
            total_dy += ms.dy;
        }

        /* Mouse Horizontal Movement = Turning */
        if (total_dx != 0 || total_dy != 0) {
            event_t me;
            memset(&me, 0, sizeof(me));
            me.type = ev_mouse;
            me.data1 = (cur_mbtn & 1) | ((cur_mbtn & 2) ? 2 : 0);
            me.data2 = total_dx * 6; /* 6x sensitivity for turning */
            me.data3 = 0;
            D_PostEvent(&me);
        }

        /* Left click = FIRE */
        if ((cur_mbtn & 1) && !(prev_mbtn & 1)) {
            press_key_decay(KEY_FIRE, 150);
        } else if (!(cur_mbtn & 1) && (prev_mbtn & 1)) {
            queue_key(0, KEY_FIRE);
            key_held_until[KEY_FIRE] = 0;
        }

        /* Right click = USE */
        if ((cur_mbtn & 2) && !(prev_mbtn & 2)) {
            press_key_decay(KEY_USE, 150);
        } else if (!(cur_mbtn & 2) && (prev_mbtn & 2)) {
            queue_key(0, KEY_USE);
            key_held_until[KEY_USE] = 0;
        }

        prev_mbtn = cur_mbtn;
    }
}

void DG_DrawFrame(void) {
    static bool first_frame = true;
    if (first_frame && fb_direct) {
        first_frame = false;
        /* Clear any console text printed during engine startup */
        uint32_t pitch_words = scr_pitch / 4;
        for (uint32_t y = 0; y < scr_h; y++) {
            for (uint32_t x = 0; x < scr_w; x++) {
                if (x < (uint32_t)view_x || x >= (uint32_t)(view_x + DOOMGENERIC_RESX) ||
                    y < (uint32_t)view_y || y >= (uint32_t)(view_y + DOOMGENERIC_RESY)) {
                    fb_direct[y * pitch_words + x] = 0x0015181F; /* Dark Nord slate */
                }
            }
        }
    }

    if (fb_direct && DG_ScreenBuffer) {
        uint32_t pitch_words = scr_pitch / 4;
        for (int y = 0; y < DOOMGENERIC_RESY; y++) {
            uint32_t* dst = fb_direct + ((y + view_y) * pitch_words) + view_x;
            pixel_t* src = DG_ScreenBuffer + (y * DOOMGENERIC_RESX);
            memcpy(dst, src, DOOMGENERIC_RESX * 4);
        }
    }

    poll_inputs();
}

void DG_SleepMs(uint32_t ms) {
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    if (key_queue_head == key_queue_tail) {
        return 0; /* Queue empty */
    }
    unsigned short data = key_queue[key_queue_tail];
    key_queue_tail = (key_queue_tail + 1) % KEYQUEUE_SIZE;

    if (pressed) *pressed = (data >> 8) & 1;
    if (doomKey) *doomKey = data & 0xFF;
    return 1;
}

void DG_SetWindowTitle(const char* title) {
    (void)title;
}

int main(int argc, char** argv) {
    doomgeneric_Create(argc, argv);
    while (1) {
        doomgeneric_Tick();
    }
    return 0;
}
