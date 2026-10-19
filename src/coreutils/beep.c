/*
 * IotaOS - beep.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * beep [-f freq_hz] [-l ms] - one PC speaker tone via KIOCSOUND on the tty.
 */

#include "common.h"
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#define PIT_FREQ 1193180u

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int freq = 880;
    int ms = 250;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-f") && i + 1 < argc)      freq = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-l") && i + 1 < argc) ms   = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h")) {
            fprintf(stderr, "usage: %s [-f freq_hz] [-l ms]\n", prog);
            return 0;
        } else {
            fprintf(stderr, "%s: unknown option '%s'\n", prog, argv[i]);
            return 1;
        }
    }
    if (freq < 20 || freq > (int)(PIT_FREQ / 2) || ms <= 0) {
        fprintf(stderr, "%s: frequency 20-%u and length > 0 ms required\n",
                prog, PIT_FREQ / 2);
        return 1;
    }

    int fd = open("/dev/tty", O_WRONLY);
    if (fd < 0) fd = open("/dev/console", O_WRONLY);
    if (fd < 0) {
        warn_errno(prog, "/dev/tty");
        return 1;
    }

    unsigned period = PIT_FREQ / (unsigned)freq;
    if (period == 0) period = 1;
    ioctl(fd, KIOCSOUND, (void *)(uintptr_t)period);
    usleep((unsigned)ms * 1000u);
    ioctl(fd, KIOCSOUND, (void *)0);
    close(fd);
    return 0;
}
