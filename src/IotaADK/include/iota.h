/*
 * IotaOS ADK - iota.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef IOTA_H
#define IOTA_H

#include <stdint.h>

/* IOTA_RELOC dynamically calculates physical load addresses at runtime.
 * Mandatory for all static strings and global variables until we build Virtual Memory! */
#define IOTA_RELOC(ptr) ({ \
    uintptr_t __base; \
    __asm__ volatile ( \
        "call 1f\n" \
        "1: pop %0\n" \
        "sub $1b, %0\n" \
        : "=r"(__base) \
    ); \
    (typeof(&(ptr)[0]))((uintptr_t)(ptr) + __base); \
})

/* --- Syscall Wrappers --- */

static inline void iota_print_str(const char* str) {
    /* We map 'str' to the "c" (ecx) register to match your kernel's SYS_PRINT */
    __asm__ volatile ("int $0x80" : : "a"(1), "c"(str) : "memory");
}

static inline void iota_ls() {
    __asm__ volatile ("int $0x80" : : "a"(8) : "memory");
}

static inline int iota_exec(const char* filename) {
    int return_code;
    __asm__ volatile ("int $0x80" : "=a"(return_code) : "a"(7), "b"(filename) : "memory");
    return return_code;
}

static inline void iota_reboot() {
    __asm__ volatile ("int $0x80" : : "a"(2) : "memory");
}

static inline void iota_exit(int exit_code) {
    __asm__ volatile ("int $0x80" : : "a"(6), "b"(exit_code) : "memory");
}

static inline void iota_meminfo(uint32_t* free, uint32_t* total) {
    uint32_t f, t;
    __asm__ volatile ("int $0x80" : "=a"(f), "=b"(t) : "a"(5) : "memory");
    if(free) *free = f;
    if(total) *total = t;
}

static inline void iota_get_line(char* buf, int size) {
    __asm__ volatile ("int $0x80" : : "a"(3), "b"(buf), "c"(size) : "memory");
}

static inline void iota_clear() {
    __asm__ volatile ("int $0x80" : : "a"(4) : "memory");
}

static inline int iota_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static inline uint32_t iota_strlen(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}

/* --- TASK MANAGEMENT SYSCALLS --- */

static inline void iota_sleep(uint32_t ms) {
    __asm__ volatile ("int $0x80" : : "a"(9), "b"(ms) : "memory");
}

static inline void iota_yield(void) {
    __asm__ volatile ("int $0x80" : : "a"(10) : "memory");
}

static inline int iota_spawn(const char* filename) {
    int return_code;
    __asm__ volatile ("int $0x80" : "=a"(return_code) : "a"(11), "b"(filename) : "memory");
    return return_code;
}

static inline void iota_putchar_at(char c, uint8_t color, uint32_t x, uint32_t y) {
    uint32_t coords = (x << 16) | (y & 0xFFFF);
    __asm__ volatile ("int $0x80" : : "a"(12), "b"((uint32_t)c), "c"((uint32_t)color), "d"(coords) : "memory");
}

static inline void iota_stress_ram(uint32_t mode, uint32_t mb) {
    __asm__ volatile ("int $0x80" : : "a"(13), "b"(mode), "c"(mb) : "memory");
}

#endif