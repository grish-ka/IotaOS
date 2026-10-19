/*
 * IotaOS - diag.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>

/* What the kernel knows about how it booted, shown on the diagnostics screen */
typedef struct {
    uint32_t total_memory;      /* bytes the kernel uses */
    uint64_t detected_memory;   /* bytes the bootloader reported */
    uint32_t initrd_address;
    uint32_t initrd_size;
    uint32_t fs_entries;
    uint32_t multiboot_flags;
} diag_boot_info_t;

/* Keys that open the diagnostics screen during the boot window */
#define BOOT_KEY_DEL 0x53
#define BOOT_KEY_ESC 0x01
#define BOOT_KEY_F2  0x3C

/* Set by the keyboard driver: while boot_key_phase is set, key presses are
   recorded in boot_key (a set 1 scancode, 0 = none) instead of typed */
extern volatile int boot_key_phase;
extern volatile int boot_key;

/* Wait about `seconds` showing a countdown; returns 1 if a diagnostics key was pressed */
int diag_boot_window(int seconds);

/* The full-screen diagnostics page (continue, reboot or halt from it) */
void diag_run(const diag_boot_info_t *info);

#endif
