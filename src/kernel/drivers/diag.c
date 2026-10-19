/*
 * IotaOS - diag.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * A boot-time diagnostics screen: press DEL (or Esc, or F2) while the boot
 * messages scroll to stop here and read what the kernel found: CPU, memory,
 * clock, interrupt controller, devices. Useful for real hardware, where there
 * is no emulator monitor to ask.
 */

#include "drivers/diag.h"
#include "drivers/terminal.h"
#include "drivers/stdio.h"
#include "drivers/string.h"
#include "drivers/timer.h"
#include "drivers/rtc.h"
#include "drivers/system.h"
#include "drivers/io.h"
#include "mem/pmm.h"
#include "cpu/task.h"

volatile int boot_key_phase = 0;
volatile int boot_key = 0;

static void copy_bytes(char *dst, const void *src, int n) {
    const char *s = src;
    for (int i = 0; i < n; i++) dst[i] = s[i];
}

static int is_diag_key(int key) {
    return key == BOOT_KEY_DEL || key == BOOT_KEY_ESC || key == BOOT_KEY_F2;
}

int diag_boot_window(int seconds) {
    boot_key = 0;
    boot_key_phase = 1;
    int hit = 0;
    for (int tenth = seconds * 10; tenth > 0 && !hit; tenth--) {
        printf("\rPress \033[1mDEL\033[0m for diagnostics (continuing in %d)...  ", (tenth + 9) / 10);
        timer_sleep(10); /* 100 ms at 100 Hz */
        hit = is_diag_key(boot_key);
    }
    printf("\r\033[K");
    if (!hit) boot_key_phase = 0;
    return hit;
}

/* ---- CPU ---- */

static int has_cpuid(void) {
    uint32_t before, after;
    __asm__ volatile(
        "pushfl\n pushfl\n"
        "xorl $0x200000, (%%esp)\n"   /* flip the ID bit */
        "popfl\n pushfl\n"
        "popl %0\n popl %1\n"
        : "=r"(after), "=r"(before) : : "cc");
    /* Restore the flags we started with */
    __asm__ volatile("pushl %0\n popfl" : : "r"(before) : "cc");
    return (before ^ after) & 0x200000;
}

static void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(0));
}

static void show_cpu(void) {
    printf("\033[1;33mCPU\033[0m\n");
    if (!has_cpuid()) {
        printf("  Pre-Pentium processor (no CPUID instruction)\n");
        return;
    }
    uint32_t a, b, c, d, max;
    cpuid(0, &max, &b, &c, &d);
    char vendor[13];
    copy_bytes(vendor, &b, 4);
    copy_bytes(vendor + 4, &d, 4);
    copy_bytes(vendor + 8, &c, 4);
    vendor[12] = '\0';

    cpuid(1, &a, &b, &c, &d);
    uint32_t family = (a >> 8) & 0xF, model = (a >> 4) & 0xF, stepping = a & 0xF;
    if (family == 0xF) family += (a >> 20) & 0xFF;
    if (family == 6 || family == 0xF) model += ((a >> 16) & 0xF) << 4;

    char brand[49] = "";
    cpuid(0x80000000, &a, &b, &c, &d);
    if (a >= 0x80000004) {
        uint32_t *w = (uint32_t *)brand;
        for (uint32_t leaf = 0; leaf < 3; leaf++) {
            cpuid(0x80000002 + leaf, &w[leaf * 4], &w[leaf * 4 + 1], &w[leaf * 4 + 2], &w[leaf * 4 + 3]);
        }
        brand[48] = '\0';
    }
    const char *name = brand;
    while (*name == ' ') name++;

    printf("  %s%s%s\n", name, *name ? "  " : "", vendor);
    printf("  family %d, model %d, stepping %d\n", family, model, stepping);

    cpuid(1, &a, &b, &c, &d);
    printf("  features:");
    static const struct { int bit; const char *name; } edx_flags[] = {
        {0, "fpu"}, {4, "tsc"}, {5, "msr"}, {6, "pae"}, {8, "cx8"}, {9, "apic"},
        {15, "cmov"}, {23, "mmx"}, {24, "fxsr"}, {25, "sse"}, {26, "sse2"},
    };
    for (unsigned i = 0; i < sizeof(edx_flags) / sizeof(edx_flags[0]); i++) {
        if (d & (1u << edx_flags[i].bit)) printf(" %s", edx_flags[i].name);
    }
    if (c & 1) printf(" sse3");
    if (c & (1u << 9)) printf(" ssse3");
    if (c & (1u << 19)) printf(" sse4.1");
    if (c & (1u << 20)) printf(" sse4.2");
    printf("\n");
}

/* ---- the rest ---- */

static void mb(char *out, uint64_t bytes) {
    sprintf(out, "%u MB", (uint32_t)(bytes / (1024 * 1024)));
}

static void show_memory(const diag_boot_info_t *info) {
    char used_by_kernel[24], detected[24];
    mb(detected, info->detected_memory);
    mb(used_by_kernel, (uint64_t)info->total_memory);
    uint32_t total = pmm_get_total_block_count(), free_blocks = pmm_get_free_block_count();
    printf("\033[1;33mMemory\033[0m\n");
    printf("  bootloader reported %s, kernel uses %s%s\n", detected, used_by_kernel,
           (uint64_t)info->total_memory < info->detected_memory ? " (2 GB active 32-bit limit)" : "");
    printf("  %d pages of 4 KB: %d free, %d in use\n", total, free_blocks, total - free_blocks);
    printf("  ramdisk at 0x%x, %d KB, %d entries\n", info->initrd_address, info->initrd_size / 1024, info->fs_entries);
}

static void two(char *out, uint32_t v) {
    out[0] = '0' + (v / 10) % 10;
    out[1] = '0' + v % 10;
    out[2] = '\0';
}

static void show_clock(void) {
    uint32_t now = rtc_now(), secs = now % 86400, days = now / 86400;
    char h[3], m[3], s[3];
    two(h, secs / 3600);
    two(m, (secs / 60) % 60);
    two(s, secs % 60);
    uint32_t ticks = timer_get_ticks();
    printf("\033[1;33mClock\033[0m\n");
    printf("  %s:%s:%s UTC (epoch day %d), %d timer ticks since boot\n", h, m, s, days, ticks);
}

static void show_hardware(void) {
    uint8_t mask1 = inb(0x21), mask2 = inb(0xA1);
    printf("\033[1;33mInterrupts and devices\033[0m\n");
    printf("  PIC masks 0x%x/0x%x (a 0 bit = on: timer IRQ0, keyboard IRQ1)\n", mask1, mask2);
    printf("  VGA text 80x25, PS/2 keyboard, PIT timer, CMOS clock\n");
}

void diag_run(const diag_boot_info_t *info) {
    terminal_clear();
    printf("\033[1;37;44m IotaOS diagnostics                                                           \033[0m\n");
    printf("Multiboot flags 0x%x. Nothing is running yet: init starts when you continue.\n\n", info->multiboot_flags);
    show_cpu();
    printf("\n");
    show_memory(info);
    printf("\n");
    show_clock();
    printf("\n");
    show_hardware();

    printf("\n\033[7m ENTER \033[0m continue booting   \033[7m R \033[0m reboot   \033[7m H \033[0m halt (keep this screen)\n");

    for (;;) {
        boot_key = 0;
        /* timer_sleep() leaves interrupts off: enable them so keys can arrive */
        while (!boot_key) __asm__ volatile("sti; hlt" : : : "memory");
        int key = boot_key;
        if (key == 0x1C) break;                 /* Enter */
        if (key == 0x13) reboot();              /* R */
        if (key == 0x23) {                      /* H: stay, refreshing nothing */
            printf("\nHalted. Press the reset button or power off.\n");
            for (;;) __asm__ volatile("cli; hlt");
        }
    }
    boot_key_phase = 0;
    terminal_clear();
}
