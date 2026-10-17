/*
* IotaOS - termios.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stddef.h>

/*
 * The kernel tty only stores the four flag words; control characters are
 * fixed, so c_cc is reported with the values the console actually produces.
 */
int tcgetattr(int fd, struct termios *termios_p) {
    if (!termios_p) return -1;
    memset(termios_p, 0, sizeof(*termios_p));
    termios_p->c_cc[VINTR] = 0x03;   /* ^C */
    termios_p->c_cc[VQUIT] = 0x1c;   /* ^\ */
    termios_p->c_cc[VERASE] = '\b';  /* the keyboard driver sends backspace as ^H */
    termios_p->c_cc[VKILL] = 0x15;   /* ^U */
    termios_p->c_cc[VEOF] = 0x04;    /* ^D */
    termios_p->c_cc[VMIN] = 1;
    termios_p->c_cc[VTIME] = 0;
    termios_p->c_cc[VSUSP] = 0x1a;   /* ^Z */
    termios_p->c_cc[VWERASE] = 0x17; /* ^W */
    termios_p->c_cc[VLNEXT] = 0x16;  /* ^V */
    return ioctl(fd, TCGETS, termios_p) < 0 ? -1 : 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    (void)optional_actions;
    if (!termios_p) return -1;
    return ioctl(fd, TCSETS, (void *)termios_p) < 0 ? -1 : 0;
}

int tcflow(int fd, int action) {
    (void)fd;
    (void)action;
    /* The console has no flow control */
    return 0;
}

/* The console has no line speed; report 38400 baud like a Linux virtual console */
speed_t cfgetispeed(const struct termios *termios_p) {
    (void)termios_p;
    return B38400;
}

speed_t cfgetospeed(const struct termios *termios_p) {
    (void)termios_p;
    return B38400;
}

int cfsetispeed(struct termios *termios_p, speed_t speed) {
    (void)termios_p;
    (void)speed;
    return 0;
}

int cfsetospeed(struct termios *termios_p, speed_t speed) {
    (void)termios_p;
    (void)speed;
    return 0;
}

void cfmakeraw(struct termios *t) {
    t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    t->c_oflag &= ~OPOST;
    t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t->c_cflag &= ~(CSIZE | PARENB);
    t->c_cflag |= CS8;
    t->c_cc[VMIN] = 1;
    t->c_cc[VTIME] = 0;
}

int tcdrain(int fd) {
    (void)fd;
    return 0;
}

int tcflush(int fd, int queue_selector) {
    return ioctl(fd, 0x540B /* TCFLSH */, (void *)queue_selector) < 0 ? -1 : 0;
}

int tcsendbreak(int fd, int duration) {
    (void)fd;
    (void)duration;
    return 0;
}
