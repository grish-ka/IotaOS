/*
 * IotaOS - devices.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "drivers/devices.h"
#include "drivers/tty.h"
#include "drivers/timer.h"

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

    /* One VGA console: tty, tty1 and console are all the same device */
    vfs_node_t* console = make_device(dev, "console", 0620);
    tty_init(console);
    tty_init(make_device(dev, "tty", 0666));
    tty_init(make_device(dev, "tty1", 0620));
    return console;
}
