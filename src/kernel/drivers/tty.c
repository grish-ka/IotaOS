/*
 * IotaOS - tty.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "drivers/audio.h"
#include "tty.h"
#include "terminal.h"
#include "cpu/task.h"

static struct termios current_termios = {
    .c_iflag = 0, .c_oflag = OPOST | ONLCR, .c_cflag = 0,
    .c_lflag = ICANON | ECHO | ISIG
};

#define TTY_BUF_SIZE 1024
static char tty_buf[TTY_BUF_SIZE];
/* Written by the keyboard IRQ while tty_read() sleeps, so they must be re-read */
static volatile int tty_head = 0;
static volatile int tty_tail = 0;
static volatile int tty_lines = 0;
static volatile int tty_eof = 0;     /* pending Ctrl-D end-of-file marks */
static volatile int tty_line_start = 0; /* where the line being edited begins */

static vfs_node_t* tty_node;

/* Foreground process group, as set with tcsetpgrp(); kept for programs
   that hand the terminal to a child and wait until it owns it */
static int tty_fg_pgrp = 1;

static void tty_echo(char c) {
    if (current_termios.c_lflag & ECHO) {
        terminal_putchar(c);
        terminal_flush_cursor();
    }
}

static void tty_discard_line(void) {
    tty_head = tty_line_start;
}

void tty_input_char(char c) {
    int canonical = current_termios.c_lflag & ICANON;

    if ((current_termios.c_lflag & ISIG) && c == 0x03) { /* Ctrl-C */
        int killed = task_interrupt_foreground();
        if (canonical) {
            tty_discard_line();
            if (current_termios.c_lflag & ECHO) terminal_writestring("^C\n");
            return;
        }
        /* Raw mode with nothing to interrupt (e.g. a shell prompt): there is no
           signal delivery, so hand ^C to the reader as an ordinary key */
        if (killed) return;
    }

    if (canonical) {
        /* Line editing: backspace and Ctrl-U stay within the current line */
        if (c == '\b' || c == 0x7f) {
            if (tty_head != tty_line_start) {
                tty_head = (tty_head - 1 + TTY_BUF_SIZE) % TTY_BUF_SIZE;
                if (current_termios.c_lflag & ECHO) terminal_writestring("\b \b");
            }
            return;
        }
        if (c == 0x15) { /* Ctrl-U */
            while (tty_head != tty_line_start) {
                tty_head = (tty_head - 1 + TTY_BUF_SIZE) % TTY_BUF_SIZE;
                if (current_termios.c_lflag & ECHO) terminal_writestring("\b \b");
            }
            return;
        }
        if (c == 0x04) { /* Ctrl-D: end of file, or push out a partial line */
            if (tty_head == tty_line_start) tty_eof++;
            else tty_lines++;
            tty_line_start = tty_head;
            return;
        }
    }

    int next = (tty_head + 1) % TTY_BUF_SIZE;
    if (next == tty_tail) return; /* buffer full: drop the key */
    tty_buf[tty_head] = c;
    tty_head = next;
    tty_echo(c);

    if (c == '\n') {
        tty_lines++;
        tty_line_start = tty_head;
    } else if (!canonical) {
        tty_line_start = tty_head;
    }
}

/* Multi-byte key sequences (arrows etc.) are only meaningful to raw-mode readers */
void tty_input_seq(const char *seq) {
    if (current_termios.c_lflag & ICANON) return;
    while (*seq) tty_input_char(*seq++);
}

/* Would a read() return now? (a complete line in canonical mode, any byte in raw mode) */
int tty_readable(void) {
    if (current_termios.c_lflag & ICANON) return tty_lines > 0 || tty_eof > 0;
    return tty_head != tty_tail;
}

static uint32_t tty_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    uint32_t read = 0;

    __asm__ volatile("sti");
    while (1) {
        if (current_termios.c_lflag & ICANON) {
            if (tty_lines > 0) break;
            if (tty_eof > 0 && tty_tail == tty_line_start) {
                tty_eof--;
                __asm__ volatile("cli");
                return 0;
            }
        } else {
            if (tty_head != tty_tail) break;
        }
        task_cpu_idle = 1;
        __asm__ volatile("hlt" : : : "memory");
        task_cpu_idle = 0;
    }
    __asm__ volatile("cli");

    while (read < size && tty_tail != tty_head) {
        if ((current_termios.c_lflag & ICANON) && tty_tail == tty_line_start && tty_lines == 0) break;
        char c = tty_buf[tty_tail];
        tty_tail = (tty_tail + 1) % TTY_BUF_SIZE;
        buffer[read++] = c;
        if (c == '\n') {
            /* Keep the line count right in raw mode too, for when ICANON returns */
            tty_lines--;
            if (current_termios.c_lflag & ICANON) break;
        }
    }
    /* A Ctrl-D-terminated partial line ends here without a newline */
    if ((current_termios.c_lflag & ICANON) && read > 0 && buffer[read - 1] != '\n' &&
        tty_tail == tty_line_start && tty_lines > 0) {
        tty_lines--;
    }
    return read;
}

static uint32_t tty_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    /* Output processing: with OPOST and ONLCR (the default), "\n" becomes "\r\n" */
    int onlcr = (current_termios.c_oflag & (OPOST | ONLCR)) == (OPOST | ONLCR);
    for (uint32_t i = 0; i < size; i++) {
        if (onlcr && buffer[i] == '\n') terminal_emit('\r');
        terminal_emit(buffer[i]);
    }
    terminal_flush_cursor();
    return size;
}

static int tty_ioctl(vfs_node_t *node, uint32_t request, void *arg) {
    (void)node;
    struct termios *t = (struct termios *)arg;

    switch (request) {
    case TCGETS:
        *t = current_termios;
        return 0;
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
        current_termios = *t;
        return 0;
    case TCFLSH:
        return 0;
    case TIOCGWINSZ: {
        uint16_t* ws = (uint16_t*)arg; /* rows, cols, xpixel, ypixel */
        ws[0] = 25;
        ws[1] = 80;
        ws[2] = 0;
        ws[3] = 0;
        return 0;
    }
    case TIOCSWINSZ:
        return 0;
    case TIOCGPGRP:
        *(int*)arg = tty_fg_pgrp;
        return 0;
    case TIOCSPGRP:
        tty_fg_pgrp = *(int*)arg;
        return 0;
    case FIONREAD:
        *(int*)arg = (tty_head - tty_tail + TTY_BUF_SIZE) % TTY_BUF_SIZE;
        return 0;
    case 0x4BC0: /* KIOCSOUND: PC speaker, arg = PIT period (1193180/Hz); 0 stops.
                     Duration is controlled by the caller, as on Linux. */ {
        uint32_t period = (uint32_t)(uintptr_t)arg;
        if (period) speaker_beep(1193180u / period);
        else        speaker_off();
        return 0;
    }
    }
    return -25; /* ENOTTY */
}

/* Terminal replies (cursor position reports) arrive as if typed */
static void tty_respond(const char* s) {
    while (*s) tty_input_char(*s++);
}

void tty_init(vfs_node_t* node) {
    tty_node = node;
    terminal_set_responder(tty_respond);
    node->flags = FS_CHARDEVICE;
    node->mode = 0620;
    node->read = &tty_read;
    node->write = &tty_write;
    node->ioctl = &tty_ioctl;
}

/* Provide getchar for the kernel's stdio gets() function */
char getchar(void) {
    char c = 0;
    tty_read(tty_node, 0, 1, (uint8_t*)&c);
    return c;
}
