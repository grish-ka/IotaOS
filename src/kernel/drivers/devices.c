/*
 * IotaOS - devices.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "drivers/devices.h"
#include "drivers/tty.h"
#include "drivers/timer.h"
#include "drivers/fb.h"
#include "drivers/mouse.h"
#include "drivers/audio.h"
#include "drivers/ata.h"

static uint32_t null_read(vfs_node_t* n, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)n; (void)off; (void)size; (void)buf;
    return 0;
}

static uint32_t sink_write(vfs_node_t* n, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)n; (void)off; (void)buf;
    return size;
}

static uint32_t zero_read(vfs_node_t* n, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)n; (void)off;
    for (uint32_t i = 0; i < size; i++) buf[i] = 0;
    return size;
}

/* xorshift, reseeded from the timer: fine for mktemp and $RANDOM, not for crypto */
static uint32_t random_state = 2463534242u;

static uint32_t random_read(vfs_node_t* n, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)n; (void)off;
    random_state ^= timer_get_ticks();
    for (uint32_t i = 0; i < size; i++) {
        random_state ^= random_state << 13;
        random_state ^= random_state >> 17;
        random_state ^= random_state << 5;
        buf[i] = (uint8_t)random_state;
    }
    return size;
}

static vfs_node_t* make_device(vfs_node_t* dev, const char* name, uint32_t mode) {
    vfs_node_t* node = vfs_finddir(dev, name);
    if (!node) node = vfs_create(dev, name, FS_CHARDEVICE, mode, 0);
    if (node) {
        node->flags = FS_CHARDEVICE;
        node->mode = mode;
    }
    return node;
}

static uint32_t fb_dev_read(vfs_node_t* n, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)n;
    if (!fb_info.enabled || !fb_info.address) return 0;
    if (off >= fb_info.size) return 0;
    if (off + size > fb_info.size) size = fb_info.size - off;
    uint8_t* src = (uint8_t*)fb_info.address + off;
    for (uint32_t i = 0; i < size; i++) buf[i] = src[i];
    return size;
}

static uint32_t fb_dev_write(vfs_node_t* n, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)n;
    if (!fb_info.enabled || !fb_info.address) return 0;
    if (off >= fb_info.size) return 0;
    if (off + size > fb_info.size) size = fb_info.size - off;
    uint8_t* dst = (uint8_t*)fb_info.address + off;
    for (uint32_t i = 0; i < size; i++) dst[i] = buf[i];
    return size;
}

static int fb_dev_ioctl(vfs_node_t* n, uint32_t request, void* arg) {
    (void)n;
    if (!fb_info.enabled || !arg) return -22; /* EINVAL */
    if (request == 0x4600) { /* FBIOGET_VSCREENINFO */
        uint32_t* info = (uint32_t*)arg;
        info[0] = fb_info.width;
        info[1] = fb_info.height;
        info[2] = fb_info.pitch;
        info[3] = fb_info.bpp;
        info[4] = fb_info.paddr;
        info[5] = fb_info.size;
        return 0;
    }
    return -25; /* ENOTTY */
}

static uint32_t mouse_dev_read(vfs_node_t* n, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)n; (void)off;
    if (!buf || size < 12) return 0;
    mouse_event_t ev;
    if (!mouse_pop_event(&ev)) return 0;
    uint32_t to_copy = (size < sizeof(mouse_event_t)) ? size : sizeof(mouse_event_t);
    uint8_t* src = (uint8_t*)&ev;
    for (uint32_t i = 0; i < to_copy; i++) buf[i] = src[i];
    return to_copy;
}

static int mouse_dev_ioctl(vfs_node_t* n, uint32_t request, void* arg) {
    (void)n;
    if (request == 4) {
        return mouse_is_vm();
    }
    if (!arg) return -22;
    if (request == 1) {
        mouse_get_extended_state((mouse_state_t*)arg);
        return 0;
    } else if (request == 2) {
        int sens = *(int*)arg;
        mouse_set_sensitivity(sens);
        return 0;
    } else if (request == 3) {
        int* pos = (int*)arg;
        mouse_set_position(pos[0], pos[1]);
        return 0;
    }
    return -25;
}

/* In-memory sector buffer for fallback when no physical/virtual ATA disk is attached */
#define HDA_FALLBACK_SECTORS 128
static uint8_t hda_fallback_buf[HDA_FALLBACK_SECTORS * ATA_SECTOR_SIZE] = {0};

static uint32_t hda_dev_read(vfs_node_t* n, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)n;
    if (!buf || size == 0) return 0;

    if (!ata_primary_master.present) {
        if (off >= sizeof(hda_fallback_buf)) return 0;
        if (off + size > sizeof(hda_fallback_buf)) size = sizeof(hda_fallback_buf) - off;
        for (uint32_t i = 0; i < size; i++) buf[i] = hda_fallback_buf[off + i];
        return size;
    }

    uint32_t start_sec = off / ATA_SECTOR_SIZE;
    uint32_t sec_offset = off % ATA_SECTOR_SIZE;
    uint32_t bytes_left = size;
    uint32_t bytes_read = 0;
    uint8_t sec_buf[ATA_SECTOR_SIZE];

    while (bytes_left > 0 && start_sec < ata_primary_master.sectors) {
        if (ata_read_sectors(start_sec, 1, sec_buf) != 0) break;
        uint32_t to_copy = ATA_SECTOR_SIZE - sec_offset;
        if (to_copy > bytes_left) to_copy = bytes_left;
        for (uint32_t i = 0; i < to_copy; i++) buf[bytes_read + i] = sec_buf[sec_offset + i];
        bytes_read += to_copy;
        bytes_left -= to_copy;
        start_sec++;
        sec_offset = 0;
    }
    return bytes_read;
}

static uint32_t hda_dev_write(vfs_node_t* n, uint32_t off, uint32_t size, uint8_t* buf) {
    (void)n;
    if (!buf || size == 0) return 0;

    if (!ata_primary_master.present) {
        if (off >= sizeof(hda_fallback_buf)) return 0;
        if (off + size > sizeof(hda_fallback_buf)) size = sizeof(hda_fallback_buf) - off;
        for (uint32_t i = 0; i < size; i++) hda_fallback_buf[off + i] = buf[i];
        return size;
    }

    uint32_t start_sec = off / ATA_SECTOR_SIZE;
    uint32_t sec_offset = off % ATA_SECTOR_SIZE;
    uint32_t bytes_left = size;
    uint32_t bytes_written = 0;
    uint8_t sec_buf[ATA_SECTOR_SIZE];

    while (bytes_left > 0 && start_sec < ata_primary_master.sectors) {
        uint32_t to_copy = ATA_SECTOR_SIZE - sec_offset;
        if (to_copy > bytes_left) to_copy = bytes_left;

        if (sec_offset != 0 || to_copy < ATA_SECTOR_SIZE) {
            /* Partial sector: read-modify-write */
            if (ata_read_sectors(start_sec, 1, sec_buf) != 0) break;
            for (uint32_t i = 0; i < to_copy; i++) sec_buf[sec_offset + i] = buf[bytes_written + i];
            if (ata_write_sectors(start_sec, 1, sec_buf) != 0) break;
        } else {
            /* Full sector: direct write */
            if (ata_write_sectors(start_sec, 1, buf + bytes_written) != 0) break;
        }

        bytes_written += to_copy;
        bytes_left -= to_copy;
        start_sec++;
        sec_offset = 0;
    }
    return bytes_written;
}

static int hda_dev_ioctl(vfs_node_t* n, uint32_t request, void* arg) {
    (void)n;
    if (!arg) return -22;
    if (request == 0x5300 || request == 1) {
        uint32_t* info = (uint32_t*)arg;
        info[0] = ata_primary_master.present ? 1 : 0;
        info[1] = ata_primary_master.present ? ata_primary_master.sectors : HDA_FALLBACK_SECTORS;
        info[2] = ata_primary_master.present ? ata_primary_master.size_mb : 1;
        return 0;
    }
    return -25;
}

vfs_node_t* devices_init(void) {
    vfs_node_t* dev = vfs_mkdir_p("/dev");

    vfs_node_t* null = make_device(dev, "null", 0666);
    null->read = null_read;
    null->write = sink_write;

    vfs_node_t* zero = make_device(dev, "zero", 0666);
    zero->read = zero_read;
    zero->write = sink_write;

    vfs_node_t* rnd = make_device(dev, "random", 0666);
    rnd->read = random_read;
    rnd->write = sink_write;
    vfs_node_t* urnd = make_device(dev, "urandom", 0666);
    urnd->read = random_read;
    urnd->write = sink_write;

    if (fb_info.enabled) {
        vfs_node_t* fb_node = make_device(dev, "fb0", 0666);
        if (fb_node) {
            fb_node->read = fb_dev_read;
            fb_node->write = fb_dev_write;
            fb_node->ioctl = fb_dev_ioctl;
            fb_node->size = fb_info.size;
        }
    }

    vfs_node_t* mouse_node = make_device(dev, "mouse", 0666);
    if (mouse_node) {
        mouse_node->read = mouse_dev_read;
        mouse_node->ioctl = mouse_dev_ioctl;
        mouse_node->size = sizeof(mouse_state_t);
    }

    /* Register persistent ATA hard disk block device */
    vfs_node_t* hda = make_device(dev, "hda", 0660);
    if (hda) {
        hda->flags = FS_BLOCKDEVICE;
        hda->read = hda_dev_read;
        hda->write = hda_dev_write;
        hda->ioctl = hda_dev_ioctl;
        hda->size = ata_primary_master.present ? (ata_primary_master.sectors * ATA_SECTOR_SIZE) : (HDA_FALLBACK_SECTORS * ATA_SECTOR_SIZE);
    }


    /* SB16 PCM (/dev/dsp, /dev/audio, /dev/mixer) if the card is present */
    audio_init(dev);
    /* One VGA console: tty, tty1 and console are all the same device */
    vfs_node_t* console = make_device(dev, "console", 0620);
    tty_init(console);
    tty_init(make_device(dev, "tty", 0666));
    tty_init(make_device(dev, "tty1", 0620));
    return console;
}
