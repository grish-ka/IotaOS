/*
 * IotaOS - play.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * play [freq_hz] [secs] - sine tone through /dev/dsp (OSS ioctls,
 * 16-bit signed mono at 22050 Hz). All arithmetic is integer: a 512-entry
 * quarter-wave table is built with Bhaskara's sine approximation and read
 * through a 16.16 fixed-point phase accumulator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#define RATE 22050
#define TABLE_N 512 /* quarter-wave entries */

static int16_t quarter[TABLE_N];

static void build_table(void) {
    /* Bhaskara I on u = theta/pi in [0, 0.5]:
       sin(theta) ~= 16u(1-u) / (5 - 4u(1-u)); exact 1.0 at theta = pi/2.
       With a = i(1024-i), u = i/1024, so sin = 16a / (5*1024^2 - 4a). */
    for (int i = 0; i < TABLE_N; i++) {
        long long a = (long long)i * (1024 - i);
        long long den = 5LL * 1024 * 1024 - 4 * a;
        quarter[i] = (int16_t)((16 * a * 32767 + den / 2) / den);
    }
}

static int16_t sample_at(uint32_t phase) { /* phase: 16.16, one unit = full circle */
    int quad = (int)(phase >> 14) & 3;
    int idx = (int)(phase >> 5) & (TABLE_N - 1); /* quadrant spans 2^14 phase units */
    int16_t v = (quad & 1) ? quarter[TABLE_N - 1 - idx] : quarter[idx];
    return quad >= 2 ? (int16_t)-v : v;
}

int main(int argc, char *argv[]) {
    int freq = 440;
    int secs = 2;
    int ms = 1500; /* default duration: 1.5 s */

    if (argc > 1) freq = atoi(argv[1]);
    if (argc > 2) { secs = atoi(argv[2]); ms = secs * 1000; }
    if (freq < 20 || freq > 20000 || ms <= 0) {
        fprintf(stderr, "usage: %s [freq_hz] [secs]\n", argv[0]);
        return 1;
    }

    int fd = open("/dev/dsp", O_WRONLY);
    if (fd < 0) {
        perror("play: /dev/dsp");
        return 1;
    }

    int fmt = AFMT_S16_LE;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &fmt) < 0 || fmt != AFMT_S16_LE) {
        fprintf(stderr, "play: 16-bit sample format unavailable\n");
        close(fd);
        return 1;
    }
    int rate = RATE;
    if (ioctl(fd, SNDCTL_DSP_SPEED, &rate) < 0) {
        perror("play: SNDCTL_DSP_SPEED");
        close(fd);
        return 1;
    }
    int ch = 1;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &ch) < 0 || ch != 1) {
        fprintf(stderr, "play: mono unavailable\n");
        close(fd);
        return 1;
    }

    build_table();

    uint32_t phase = 0;
    uint32_t inc = (uint32_t)((65536ULL * (unsigned)freq) / (unsigned)rate);
    uint32_t fade = (uint32_t)rate / 20;              /* 50 ms in/out fades */
    uint32_t total = (uint32_t)((uint64_t)rate * (uint64_t)ms) / 1000u;

    int16_t chunk[512];
    uint32_t done = 0;
    printf("play: %d Hz for %d ms through /dev/dsp\n", freq, ms);
    while (done < total) {
        uint32_t n = total - done;
        if (n > 512) n = 512;
        for (uint32_t i = 0; i < n; i++) {
            int32_t v = sample_at(phase);
            uint32_t pos = done + i;
            uint32_t t_in = pos < fade ? pos : fade;
            uint32_t t_out = total - pos < fade ? total - pos : fade;
            uint32_t t = t_in < t_out ? t_in : t_out;
            chunk[i] = (int16_t)((v * (int32_t)t) / (int32_t)fade);
            phase += inc; /* wraps at 2^16 = one period */
        }
        uint8_t *p = (uint8_t *)chunk;
        uint32_t bytes = n * 2;
        while (bytes > 0) {
            ssize_t w = write(fd, p, bytes);
            if (w <= 0) {
                perror("play: write");
                close(fd);
                return 1;
            }
            p += w;
            bytes -= (uint32_t)w;
        }
        done += n;
    }

    ioctl(fd, SNDCTL_DSP_SYNC, 0); /* drain the ring, then stop */
    close(fd);
    return 0;
}
