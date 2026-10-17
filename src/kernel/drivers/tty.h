/*
 * IotaOS - tty.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef TTY_H
#define TTY_H

#include <stdint.h>
#include "fs/vfs.h"

/* POSIX termios flags (values match libc's <termios.h>) */
#define ISIG   0x0001
#define ICANON 0x0002
#define ECHO   0x0008

/* c_oflag */
#define OPOST  0x0001
#define ONLCR  0x0004

/* IOCTL commands (Linux numbering, as libc's <sys/ioctl.h>) */
#define TCGETS     0x5401
#define TCSETS     0x5402
#define TCSETSW    0x5403
#define TCSETSF    0x5404
#define TCFLSH     0x540B
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define FIONREAD   0x541B

struct termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag; /* Where ICANON, ECHO and ISIG live */
};

/* Turn node into the console device */
void tty_init(vfs_node_t* node);
void tty_input_char(char c);
void tty_input_seq(const char *seq);
/* Non-zero if a read() from the console would not block */
int tty_readable(void);

#endif
