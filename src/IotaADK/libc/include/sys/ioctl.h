/*
* IotaOS - ioctl.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

struct winsize {
    unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel;
};

/* Request numbers follow Linux, matching TCGETS/TCSETS in the kernel tty driver */
#define TCGETS     0x5401
#define TCSETS     0x5402
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414

int ioctl(int fd, unsigned long request, ...);

#endif