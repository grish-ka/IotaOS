#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

/* Standard POSIX Termios Flags */
#define ICANON 0x0002
#define ECHO   0x0008

/* Standard IOCTL Commands */
#define TCGETS 0x5401
#define TCSETS 0x5402

struct termios {
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag; 
};

int ioctl(int fd, unsigned long request, ...);

#endif