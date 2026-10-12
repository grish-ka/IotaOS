/*
 * IotaOS - tty.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "tty.h"
#include "terminal.h"

static struct termios current_termios = {
    .c_iflag = 0, .c_oflag = 0, .c_cflag = 0,
    .c_lflag = ICANON | ECHO 
};

#define TTY_BUF_SIZE 1024
static char tty_buf[TTY_BUF_SIZE];
static int tty_head = 0;
static int tty_tail = 0;
static volatile int tty_lines = 0;

static vfs_node_t tty_node;

void tty_input_char(char c) {
    if (c == '\b') {
        if (tty_head != tty_tail) {
            tty_head = (tty_head - 1 + TTY_BUF_SIZE) % TTY_BUF_SIZE;
            if (current_termios.c_lflag & ECHO) terminal_putchar('\b');
        }
        return;
    }

    tty_buf[tty_head] = c;
    tty_head = (tty_head + 1) % TTY_BUF_SIZE;

    if (current_termios.c_lflag & ECHO) {
        terminal_putchar(c);
    }

    if (c == '\n') {
        tty_lines++;
    }
}

static uint32_t tty_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    uint32_t read = 0;

    while (1) {
        if (current_termios.c_lflag & ICANON) {
            if (tty_lines > 0) break;
        } else {
            if (tty_head != tty_tail) break;
        }
        __asm__ volatile("sti; hlt; cli");
    }

    while (read < size && tty_tail != tty_head) {
        char c = tty_buf[tty_tail];
        tty_tail = (tty_tail + 1) % TTY_BUF_SIZE;
        buffer[read++] = c;
        if ((current_termios.c_lflag & ICANON) && c == '\n') {
            tty_lines--;
            break;
        }
    }
    return read;
}

static uint32_t tty_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    for (uint32_t i = 0; i < size; i++) {
        terminal_putchar(buffer[i]);
    }
    return size;
}

static int tty_ioctl(vfs_node_t *node, uint32_t request, void *arg) {
    (void)node;
    struct termios *t = (struct termios *)arg;
    
    if (request == TCGETS) {
        *t = current_termios;
        return 0;
    } else if (request == TCSETS) {
        current_termios = *t;
        return 0;
    }
    return -1;
}

void tty_init(void) {
    tty_node.name[0] = 't'; tty_node.name[1] = 't'; tty_node.name[2] = 'y'; tty_node.name[3] = '\0';
    tty_node.flags = FS_CHARDEVICE;
    tty_node.read = &tty_read;
    tty_node.write = &tty_write;
    tty_node.ioctl = &tty_ioctl;
}

vfs_node_t* tty_get_vfs_node(void) {
    return &tty_node;
}

/* Provide getchar for the kernel's stdio gets() function */
char getchar(void) {
    char c = 0;
    tty_read(&tty_node, 0, 1, (uint8_t*)&c);
    return c;
}