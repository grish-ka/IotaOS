/*
 * IotaOS - sys/soundcard.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * OSS-style sound ioctls for /dev/dsp, /dev/audio and /dev/mixer,
 * plus the KIOCSOUND PC-speaker tty ioctl. Values match the Linux
 * asm/soundcard.h encodings so ported programs keep working.
 */

#ifndef _SYS_SOUNDCARD_H
#define _SYS_SOUNDCARD_H

/* Sample formats for SNDCTL_DSP_SETFMT */
#define AFMT_QUERY    0x00
#define AFMT_U8       0x04
#define AFMT_S16_LE   0x08

/* /dev/dsp and /dev/audio ioctls */
#define SNDCTL_DSP_RESET      0x4D00
#define SNDCTL_DSP_SYNC       0x4D01
#define SNDCTL_DSP_POST       0x4D0C
#define SNDCTL_DSP_HALT_INPUT 0x4D0F
#define SNDCTL_DSP_SPEED      0x40044D02  /* in/out int: samples per second */
#define SNDCTL_DSP_STEREO     0x40044D03  /* in/out int: 0 mono, 1 stereo */
#define SNDCTL_DSP_SETFMT     0x40044D05  /* in/out int: AFMT_* */
#define SNDCTL_DSP_CHANNELS   0x40044D06  /* in/out int: 1 or 2 */
#define SNDCTL_DSP_GETOSPACE  0xC0044D11  /* out int: bytes writable now */

/* /dev/mixer ioctls: MIXER_WRITE(n) sets, MIXER_READ(n) gets; the int is
   (right << 8) | left, each side 0-100 */
#define MIXER_WRITE(n)    (0x40044D00 | (n))
#define MIXER_READ(n)     (0xC0044D00 | (n))
#define DEV_MIXER         0   /* master volume */
#define DEV_PCM           14  /* PCM volume */

/* PC speaker: ioctl(fd, KIOCSOUND, period) on a tty, where
   period = 1193180 / frequency_hz; period 0 silences the speaker. */
#define KIOCSOUND         0x4BC0

#endif /* _SYS_SOUNDCARD_H */
