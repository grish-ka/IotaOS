/*
 * IotaOS - audio.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * Sound Blaster 16 (QEMU "-device sb16") PCM playback through ISA DMA,
 * exposed as OSS-style /dev/dsp + /dev/audio + /dev/mixer, plus the PIT
 * channel-2 PC speaker.
 *
 * The hardware semantics here follow QEMU's hw/audio/sb16.c and
 * hw/dma/i8257.c, which is the test target. Two places deliberately
 * differ from 1995 SB16 datasheet behavior, each marked with a comment:
 * the DSP write-status polarity and the raw-Hz rate command.
 */

#include "drivers/audio.h"
#include "drivers/io.h"
#include "drivers/timer.h"
#include "drivers/stdio.h"
#include "drivers/string.h"
#include "cpu/pic.h"
#include "cpu/task.h"
#include "fs/file.h"
#include "fs/vfs.h"

/* --- DSP and mixer registers (base 0x220; QEMU maps nothing at 0x220) --- */
#define SB_BASE         0x220
#define SB_MIXER_IDX    0x224
#define SB_MIXER_DATA   0x225
#define SB_RESET        0x226   /* write 1 then 0 resets; reads return 0xff */
#define SB_READ_DATA    0x22A   /* DSP data out */
#define SB_WRITE        0x22C   /* write: command/data; read: bit7 == 0 means READY
                                   (QEMU inverts the real-hardware polarity) */
#define SB_STATUS_ACK8  0x22E   /* read: bit7 = data available; also acks the 8-bit DMA IRQ */
#define SB_ACK16        0x22F   /* read: acks the 16-bit DMA IRQ */

/* --- 8237 ISA DMA: channel 1 (8-bit) and channel 5 (16-bit) --- */
#define DMA8_PAGE   0x83
#define DMA8_ADDR   0x02
#define DMA8_COUNT  0x03
#define DMA8_CLEARFF 0x0C
#define DMA8_MASK   0x0A
#define DMA8_MODE   0x0B
#define DMA16_PAGE  0x8B
#define DMA16_ADDR  0xC4
#define DMA16_COUNT 0xC6
#define DMA16_CLEARFF 0xD8
#define DMA16_MASK  0xD4
#define DMA16_MODE  0xD6
/* single, write (memory to device), auto-init, increment, channel index 1 */
#define DMA_MODE_AUTO 0x59
#define DMA_MASK_ON   0x05  /* chan 1 | bit 2 */
#define DMA_MASK_OFF  0x01  /* chan 1 */

/* --- OSS ioctls (Linux asm/soundcard.h encodings) and formats --- */
#define AFMT_U8       0x04
#define AFMT_S16_LE   0x08
#define SNDCTL_DSP_RESET      0x4D00
#define SNDCTL_DSP_SYNC       0x4D01
#define SNDCTL_DSP_POST       0x4D0C
#define SNDCTL_DSP_SPEED      0x40044D02
#define SNDCTL_DSP_STEREO     0x40044D03
#define SNDCTL_DSP_SETFMT     0x40044D05
#define SNDCTL_DSP_CHANNELS   0x40044D06
#define SNDCTL_DSP_HALT_INPUT 0x4D0F
#define SNDCTL_DSP_GETOSPACE  0xC0044D11
#define MIXER_WRITE_BASE      0x40044D00
#define MIXER_READ_BASE       0xC0044D00
#define DEV_MIXER             0
#define DEV_PCM               14

#define ENOTTY 25

/* PIT channel-2 input clock */
#define PIT_FREQ 1193180u

/* Kernel timer frequency (kernel.c calls timer_init(100)) */
#define TIMER_HZ 100u

/* --- Playback ring: one full auto-init DMA block --- */
#define RING_SIZE        32768u
#define START_THRESHOLD  1024        /* bytes queued before the DSP starts */

static int sb_present;
static uint8_t* ring;                /* 64 KiB-aligned: a 16-bit DMA transfer never
                                        crosses a 64 KiB boundary */
static uint32_t ring_phys;

static volatile int playing;
static int fmt = AFMT_U8;            /* OSS default: unsigned 8-bit */
static int channels = 1;
static int rate = 8000;
static uint32_t bytes_per_sec = 8000;
static volatile uint32_t write_pos;      /* bytes accepted since (re)start */
static volatile uint32_t consumed_total; /* bytes consumed, one RING_SIZE per IRQ */
static volatile uint32_t irq_tick;       /* tick of the last ring-wrap IRQ */
static int vol_l = 100, vol_r = 100;
static volatile uint32_t underruns;

/* ------------------------------------------------------------------ */
/* DSP helpers                                                          */
/* ------------------------------------------------------------------ */

static void sb_spin(uint32_t n) {
    while (n--) __asm__ volatile("pause");
}

static int sb_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(SB_WRITE) & 0x80)) return 0;
    return -1;
}

static int sb_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(SB_STATUS_ACK8) & 0x80) return 0;
    return -1;
}

static int sb_cmd(uint8_t c) {
    if (sb_wait_write()) return -1;
    outb(SB_WRITE, c);
    return 0;
}

static int sb_arg(uint8_t a) {
    return sb_cmd(a);
}

static uint8_t sb_read(void) {
    return inb(SB_READ_DATA);
}

/* Reset the DSP and require a Vibra/SB16-class version: the reset handler
   queues 0xAA, then command 0xE1 returns the version low byte (minor)
   first, then the high byte (major); QEMU's default version is 0x0405. */
static int sb16_probe(void) {
    outb(SB_RESET, 1);
    sb_spin(1000);
    outb(SB_RESET, 0);
    sb_spin(10000); /* the reset handler is synchronous in QEMU; give it slack */

    if (sb_wait_read()) return 0;
    if (sb_read() != 0xAA) return 0;

    if (sb_cmd(0xE1)) return 0;
    if (sb_wait_read()) return 0;
    (void)sb_read(); /* minor */
    if (sb_wait_read()) return 0;
    uint8_t major = sb_read();
    return major >= 4;
}

/* ------------------------------------------------------------------ */
/* ISA DMA + DSP start/stop                                             */
/* ------------------------------------------------------------------ */

static int is_16bit(void) { return fmt == AFMT_S16_LE; }

static uint8_t silence_byte(void) { return (uint8_t)(is_16bit() ? 0x00 : 0x80); }

static void dma_start_channel(int is16, uint32_t phys, uint32_t count_minus1) {
    uint16_t mask = is16 ? DMA16_MASK   : DMA8_MASK;
    uint16_t ff   = is16 ? DMA16_CLEARFF : DMA8_CLEARFF;
    uint16_t page = is16 ? DMA16_PAGE   : DMA8_PAGE;
    uint16_t addr = is16 ? DMA16_ADDR   : DMA8_ADDR;
    uint16_t cnt  = is16 ? DMA16_COUNT  : DMA8_COUNT;
    uint16_t mode = is16 ? DMA16_MODE   : DMA8_MODE;

    outb(mask, DMA_MASK_ON);          /* unplug the channel while programming */
    outb(ff, 0);                      /* reset the address/count flip-flop */
    outb(page, (uint8_t)(phys >> 16));
    outb(addr, (uint8_t)(phys & 0xFF));
    outb(addr, (uint8_t)((phys >> 8) & 0xFF));
    outb(cnt, (uint8_t)(count_minus1 & 0xFF));
    outb(cnt, (uint8_t)((count_minus1 >> 8) & 0xFF));
    outb(mode, DMA_MODE_AUTO);
    outb(mask, DMA_MASK_OFF);         /* re-arm: DREQ from the DSP starts it */
}

/* Program the DMA first, then raise DREQ with the DSP command; QEMU's
   auto-init block then feeds samples continuously, interrupting once per
   full ring wrap. */
static void audio_start(void) {
    int is16 = is_16bit();
    bytes_per_sec = (uint32_t)rate * (uint32_t)channels * (is16 ? 2u : 1u);

    dma_start_channel(is16, ring_phys,
                      is16 ? (RING_SIZE / 2) - 1 : RING_SIZE - 1);

    /* Sample rate: raw Hz as two bytes, HIGH first — the DSP's 0x41
       command reads its argument hi-then-lo (QEMU's dsp_get_hilo), and a
       swapped pair silently sets 8790 Hz instead of 22050. QEMU treats the
       value as literal Hz and clears any stale time constant; real hardware
       wants the 1 MHz/(256-rate) time constant instead — deliberate
       divergence. */
    sb_cmd(0x41);
    sb_arg((uint8_t)((rate >> 8) & 0xFF));
    sb_arg((uint8_t)(rate & 0xFF));

    /* Auto-init DMA output (bit 2 of 0xC4/0xB4), block = the whole ring.
       Data flags: bit4 signed, bit5 stereo; 0xC4 uses DMA ch1, 0xB4 ch5. */
    if (is16) {
        uint32_t words = (RING_SIZE / 2) - 1;
        sb_cmd(0xB4);
        sb_arg((uint8_t)(0x10 | (channels == 2 ? 0x20 : 0x00)));
        sb_arg((uint8_t)(words & 0xFF));
        sb_arg((uint8_t)((words >> 8) & 0xFF));
    } else {
        uint32_t bytes = RING_SIZE - 1;
        sb_cmd(0xC4);
        sb_arg((uint8_t)(channels == 2 ? 0x20 : 0x00));
        sb_arg((uint8_t)(bytes & 0xFF));
        sb_arg((uint8_t)((bytes >> 8) & 0xFF));
    }

    consumed_total = 0;
    irq_tick = timer_get_ticks();
    playing = 1;
}

static void audio_stop(void) {
    if (playing) {
        sb_cmd(is_16bit() ? 0xD5 : 0xD0); /* halt the 16-/8-bit DMA transfer */
        outb(is_16bit() ? DMA16_MASK : DMA8_MASK, DMA_MASK_ON);
    }
    playing = 0;
    if (ring) memset(ring, silence_byte(), RING_SIZE);
    write_pos = 0;
    consumed_total = 0;
}

/* Vector 37 / IRQ5: one interrupt per full ring wrap while playing.
   Both ack ports are read; QEMU clears (and lowers) only the raised one. */
void audio_irq(void) {
    (void)inb(SB_STATUS_ACK8);
    (void)inb(SB_ACK16);
    if (playing) {
        consumed_total += RING_SIZE;
        irq_tick = timer_get_ticks();
    }
}

/* ------------------------------------------------------------------ */
/* Ring accounting                                                      */
/* ------------------------------------------------------------------ */

/* Bytes the DMA is assumed to have eaten: IRQ wraps plus the 100 Hz
   tick-estimated remainder of the current wrap. */
static uint32_t consumed_est(void) {
    if (!playing) return 0;
    uint32_t dt = timer_get_ticks() - irq_tick;
    uint64_t adv = ((uint64_t)dt * bytes_per_sec) / TIMER_HZ;
    if (adv > RING_SIZE - 1) adv = RING_SIZE - 1;
    return consumed_total + (uint32_t)adv;
}

static uint32_t ring_fill(void) {
    if (!playing) return write_pos;
    uint32_t c = consumed_est();
    return write_pos > c ? write_pos - c : 0;
}

/* ------------------------------------------------------------------ */
/* /dev/dsp and /dev/audio                                              */
/* ------------------------------------------------------------------ */

/* Non-blocking short writer: copies as many whole samples as fit and
   returns the count; file_write() owns the block/retry + O_NONBLOCK loop. */
static uint32_t dsp_write(vfs_node_t* node, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)node; (void)off;

    /* Underrun: the DMA is looping already-played data — silence the ring
       so the loop plays quiet instead of repeating the old chunk. */
    if (playing && ring_fill() == 0 && write_pos) {
        memset(ring, silence_byte(), RING_SIZE);
        underruns++;
    }

    uint32_t used = ring_fill();
    uint32_t freeb = RING_SIZE - used;
    uint32_t n = size < freeb ? size : freeb;
    if (is_16bit()) n &= ~1u; /* never split a sample */

    int vol = (vol_l + vol_r) / 2; /* mixer regs are store-only in QEMU:
                                      attenuation is done here, in software */
    if (vol == 100) { /* unity gain: bulk copy, no per-byte arithmetic on the
                         hot path (doom paces its game loop on this write) */
        uint32_t first = RING_SIZE - (write_pos & (RING_SIZE - 1));
        if (first > n) first = n;
        memcpy(ring + (write_pos & (RING_SIZE - 1)), buf, first);
        if (n > first) memcpy(ring, buf + first, n - first);
    } else if (is_16bit()) {
        uint32_t samples = n / 2;
        for (uint32_t s = 0; s < samples; s++) {
            int16_t v;
            memcpy(&v, buf + s * 2, 2);
            int g = (v * vol) / 100;
            if (g > 32767) g = 32767;
            if (g < -32768) g = -32768;
            uint32_t d = (write_pos + s * 2) & (RING_SIZE - 1);
            ring[d] = (uint8_t)(g & 0xFF);
            ring[(d + 1) & (RING_SIZE - 1)] = (uint8_t)((g >> 8) & 0xFF);
        }
        n = samples * 2;
    } else {
        for (uint32_t i = 0; i < n; i++) {
            int g = (((int)buf[i] - 128) * vol) / 100 + 128;
            if (g > 255) g = 255;
            if (g < 0) g = 0;
            ring[(write_pos + i) & (RING_SIZE - 1)] = (uint8_t)g;
        }
    }

    write_pos += n;
    if (!playing && write_pos >= START_THRESHOLD) audio_start();
    return n;
}

static uint32_t dsp_read(vfs_node_t* node, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)node; (void)off; (void)size; (void)buf;
    return (uint32_t)-VFS_EINVAL; /* playback-only device */
}

static int dsp_ioctl(vfs_node_t* node, uint32_t request, void* arg) {
    (void)node;
    int* val = (int*)arg;

    switch (request) {
    case SNDCTL_DSP_RESET:
        audio_stop(); /* format, channels and rate keep their values */
        return 0;

    case SNDCTL_DSP_SYNC: {
        /* Drain everything queued (IRQ5 and the 100 Hz tick wake us),
           then stop: exactly what OSS close() semantics expect. */
        __asm__ volatile("sti");
        while (playing && write_pos > consumed_est()) {
            task_cpu_idle = 1;
            __asm__ volatile("hlt" : : : "memory");
            task_cpu_idle = 0;
        }
        __asm__ volatile("cli");
        audio_stop();
        return 0;
    }

    case SNDCTL_DSP_POST:
        return 0;

    case SNDCTL_DSP_SPEED: {
        int r = *val;
        if (r < 5000) r = 5000;
        if (r > 44999) r = 44999;
        if (playing) audio_stop();
        rate = r;
        *val = rate;
        return 0;
    }

    case SNDCTL_DSP_STEREO: {
        int want = *val ? 1 : 0;
        if (playing) audio_stop();
        channels = want ? 2 : 1;
        *val = want;
        return 0;
    }

    case SNDCTL_DSP_CHANNELS:
        if (*val != 1 && *val != 2) return -VFS_EINVAL;
        if (playing) audio_stop();
        channels = *val;
        *val = channels;
        return 0;

    case SNDCTL_DSP_SETFMT:
        if (*val != AFMT_U8 && *val != AFMT_S16_LE) {
            *val = fmt; /* echo the accepted format, OSS-style */
            return -VFS_EINVAL;
        }
        if (playing) audio_stop();
        fmt = *val;
        *val = fmt;
        return 0;

    case SNDCTL_DSP_GETOSPACE:
        *val = (int)(RING_SIZE - ring_fill());
        return 0;

    case SNDCTL_DSP_HALT_INPUT:
        return 0; /* no capture path */
    }
    return -ENOTTY;
}

static uint32_t dsp_poll(vfs_node_t* node, uint32_t events) {
    (void)node; (void)events;
    /* POLLOUT only when a whole sample fits: dsp_write never splits one,
       and a "ready" poll that yields a 0-byte write would livelock the
       blocking writer (it only sleeps when POLLOUT is clear). */
    uint32_t freeb = RING_SIZE - ring_fill();
    return freeb >= (is_16bit() ? 2u : 1u) ? POLLOUT : 0;
}

/* Single-writer device (as in OSS): last close stops playback. */
static void dsp_close(vfs_node_t* node, struct file* f) {
    (void)node; (void)f;
    audio_stop();
}

/* ------------------------------------------------------------------ */
/* /dev/mixer                                                           */
/* ------------------------------------------------------------------ */

static uint32_t mixer_read(vfs_node_t* node, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)node; (void)off; (void)size; (void)buf;
    return (uint32_t)-VFS_EINVAL;
}

/* ------------------------------------------------------------------ */
/* /dev/opl - FM music register pipe (SB16 OPL3 at 0x388/0x389)        */
/*                                                                    */
/* write() accepts (index, value) byte pairs and forwards them to the */
/* chip; the DMX-MUS player itself lives in user space.               */
/* ------------------------------------------------------------------ */

static uint32_t opl_write(vfs_node_t* node, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)node; (void)off;
    uint32_t pairs = size / 2;
    for (uint32_t i = 0; i < pairs; i++) {
        outb(0x388, buf[2 * i]);
        outb(0x389, buf[2 * i + 1]);
    }
    return pairs * 2;
}

static int mixer_ioctl(vfs_node_t* node, uint32_t request, void* arg) {
    (void)node;
    int n;

    if ((request & ~0xFFu) == MIXER_WRITE_BASE) {   /* MIXER_WRITE(n) */
        n = (int)(request & 0xFF);
        if (n != DEV_MIXER && n != DEV_PCM) return -VFS_EINVAL;
        int v = *(int*)arg;
        vol_l = v & 0xFF;
        vol_r = (v >> 8) & 0xFF;
        if (vol_l > 100) vol_l = 100;
        if (vol_r > 100) vol_r = 100;
        if (sb_present) {
            /* Program the SB16 master volume (mixer index 0x22, byte per
               channel). QEMU stores it without attenuating; real hardware
               would. Audible volume is the software gain in dsp_write(). */
            outb(SB_MIXER_IDX, 0x22);
            outb(SB_MIXER_DATA, (uint8_t)(vol_l * 63 / 100));
            outb(SB_MIXER_DATA, (uint8_t)(vol_r * 63 / 100));
        }
        return 0;
    }

    if ((request & ~0xFFu) == MIXER_READ_BASE) {    /* MIXER_READ(n) */
        n = (int)(request & 0xFF);
        if (n != DEV_MIXER && n != DEV_PCM) return -VFS_EINVAL;
        *(int*)arg = (vol_r << 8) | vol_l;
        return 0;
    }

    return -ENOTTY;
}

/* ------------------------------------------------------------------ */
/* PC speaker (PIT channel 2) — works with no SB16 present              */
/* ------------------------------------------------------------------ */

/* Nonzero = auto-off tick for the '\a' bell (see audio_tick). */
static volatile uint32_t bell_until;

void speaker_beep(uint32_t freq_hz) {
    bell_until = 0;
    if (!freq_hz) {
        speaker_off();
        return;
    }
    uint32_t div = PIT_FREQ / freq_hz;
    if (div == 0) div = 1;
    if (div > 0xFFFF) div = 0xFFFF;
    outb(0x43, 0xB6);               /* ch2, lo/hi, mode 3 square wave */
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)(div >> 8));
    outb(0x61, (uint8_t)(inb(0x61) | 0x03)); /* gate + speaker enable */
}

void speaker_off(void) {
    bell_until = 0;
    outb(0x61, (uint8_t)(inb(0x61) & ~0x03));
}

/* Terminal bell ('\a'): 200 ms of 880 Hz, auto-off via the 100 Hz tick.
   Only port writes — safe even when the echo path calls it from the
   keyboard IRQ. KIOCSOUND owns the speaker explicitly, so both manual
   entry points cancel a pending bell timeout. */
void audio_bell(void) {
    speaker_beep(880);
    bell_until = timer_get_ticks() + 20;
}

void audio_tick(void) {
    uint32_t until = bell_until;
    if (until && (int32_t)(timer_get_ticks() - until) >= 0) {
        bell_until = 0;
        outb(0x61, (uint8_t)(inb(0x61) & ~0x03));
    }
}

/* ------------------------------------------------------------------ */
/* Init                                                                 */
/* ------------------------------------------------------------------ */

/* The ring lives in the kernel image's .bss, not in PMM memory: by the
   time devices_init runs, the initrd copy has consumed all low RAM below
   ~35 MiB, and a 16-bit 8237 channel cannot reach past 16 MiB anyway.
   The kernel is multiboot-loaded near 1 MiB and identity-mapped, so this
   buffer is always DMA-reachable, deterministic, and never fragmented.
   96 KiB of slack guarantees a 64 KiB-aligned 32 KiB window (a 16-bit
   transfer must never cross a 64 KiB boundary). */
#define RING_STORAGE_SIZE (96 * 1024)
static uint8_t ring_storage[RING_STORAGE_SIZE];

static int ring_place(void) {
    uint32_t aligned = ((uint32_t)(uintptr_t)ring_storage + 0xFFFFu) & ~0xFFFFu;
    if (aligned + RING_SIZE <= (uint32_t)(uintptr_t)ring_storage + RING_STORAGE_SIZE &&
        aligned + RING_SIZE <= 0x1000000u) {
        ring = (uint8_t*)aligned;
        ring_phys = aligned;
        return 0;
    }
    return -1;
}

static vfs_node_t* make_audio_device(vfs_node_t* dev, const char* name) {
    vfs_node_t* node = vfs_finddir(dev, name);
    if (!node) node = vfs_create(dev, name, FS_CHARDEVICE, 0666, 0);
    if (node) {
        node->flags = FS_CHARDEVICE;
        node->mode = 0666;
    }
    return node;
}

void audio_init(vfs_node_t* dev) {
    if (!sb16_probe()) {
        printf("[audio] No Sound Blaster 16 at 0x220; /dev/dsp disabled\n");
        return; /* the PC speaker is reached through KIOCSOUND on ttys */
    }

    if (ring_place() != 0) {
        printf("[audio] Kernel image sits too high for ISA DMA; /dev/dsp disabled\n");
        return;
    }
    sb_present = 1;
    memset(ring, 0x80, RING_SIZE); /* U8 silence until the format is set */

    vfs_node_t* dsp = make_audio_device(dev, "dsp");
    vfs_node_t* audio = make_audio_device(dev, "audio");
    for (int i = 0; i < 2; i++) {
        vfs_node_t* n = i ? audio : dsp;
        if (!n) continue;
        n->read = dsp_read;
        n->write = dsp_write;
        n->ioctl = dsp_ioctl;
        n->poll = dsp_poll;
        n->close = dsp_close;
    }

    vfs_node_t* mixer = make_audio_device(dev, "mixer");
    if (mixer) {
        mixer->read = mixer_read;
        mixer->write = mixer_read; /* byte path is EINVAL too */
        mixer->ioctl = mixer_ioctl;
    }

    vfs_node_t* opl = make_audio_device(dev, "opl");
    if (opl) {
        opl->read = mixer_read;
        opl->write = opl_write;
    }

    pic_irq_mask(5, 0); /* audio_stop() masks the DMA channel, never the PIC */
    printf("[audio] Sound Blaster 16 at 0x220 (IRQ5, DMA1/5), ring 0x%x\n", ring_phys);
}
