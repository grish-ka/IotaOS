/*
 * IotaOS - tty.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef TTY_H
#define TTY_H

#include <stdint.h>
#include "fs/vfs.h"

/* Standard POSIX Termios Flags */
#define ICANON 0x0002
#define ECHO   0x0008

/* Standard IOCTL Commands */
#define TCGETS 0x5401
#define TCSETS 0x5402

struct termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag; /* Where ICANON and ECHO live */
};

void tty_init(void);
void tty_input_char(char c);
vfs_node_t* tty_get_vfs_node(void);

#endif