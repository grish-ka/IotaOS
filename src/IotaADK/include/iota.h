/*
* IotaOS - iota.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef IOTA_H
#define IOTA_H

#include <stdint.h>

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
    uint32_t len = 0;
    while (str[len]) len++;
    __asm__ volatile (
        "mov %0, %%ecx\n"
        "mov $1, %%eax\n"
        "mov $1, %%ebx\n"
        "int $0x80"
        : : "r"(str), "r"(len) : "eax", "ebx", "ecx"
    );
}

static inline void iota_ls() {
    __asm__ volatile ("mov $8, %%eax; int $0x80" ::: "eax");
}

static inline int iota_exec(const char* filename) {
    int return_code;
    __asm__ volatile (
        "mov %1, %%ebx\n"
        "mov $7, %%eax\n"
        "int $0x80\n"
        : "=a"(return_code) : "r"(filename) : "ebx"
    );
    return return_code;
}

static inline void iota_reboot() {
    __asm__ volatile ("mov $2, %%eax; int $0x80" ::: "eax");
}

static inline void iota_exit(int exit_code) {
    __asm__ volatile (
        "mov %0, %%ebx\n"
        "jmp _iota_force_exit\n"
        : : "r"(exit_code) : "ebx"
    );
}

static inline void iota_meminfo(uint32_t* free, uint32_t* total) {
    uint32_t f, t;
    __asm__ volatile (
        "mov $5, %%eax\n"
        "int $0x80\n"
        : "=a"(f), "=b"(t) : : 
    );
    if(free) *free = f;
    if(total) *total = t;
}

static inline void iota_get_line(char* buf, int size) {
    __asm__ volatile (
        "mov $3, %%eax\n"
        "mov %0, %%ebx\n"
        "mov %1, %%ecx\n"
        "int $0x80"
        : : "r"(buf), "r"(size) : "eax", "ebx", "ecx"
    );
}

static inline void iota_clear() {
    __asm__ volatile (
        "mov $4, %%eax\n"
        "int $0x80"
        : : : "eax"
    );
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

/* --- NEW TASK MANAGEMENT SYSCALLS --- */

static inline void iota_sleep(uint32_t ms) {
    __asm__ volatile (
        "mov %0, %%ebx\n"
        "mov $9, %%eax\n"
        "int $0x80\n"
        : : "r"(ms) : "eax", "ebx"
    );
}

static inline void iota_yield(void) {
    __asm__ volatile (
        "mov $10, %%eax\n"
        "int $0x80\n"
        : : : "eax"
    );
}

/* Spawns an app as a background task! */
static inline int iota_spawn(const char* filename) {
    int return_code;
    __asm__ volatile (
        "mov %1, %%ebx\n"
        "mov $11, %%eax\n"
        "int $0x80\n"
        : "=a"(return_code) : "r"(filename) : "ebx"
    );
    return return_code;
}

/* Draws a character at a specific X/Y coordinate without moving the cursor! */
static inline void iota_putchar_at(char c, uint8_t color, uint32_t x, uint32_t y) {
    /* Pack X and Y into a single 32-bit integer to pass to the kernel */
    uint32_t coords = (x << 16) | (y & 0xFFFF);
    __asm__ volatile (
        "mov %0, %%ebx\n"
        "mov %1, %%ecx\n"
        "mov %2, %%edx\n"
        "mov $12, %%eax\n"
        "int $0x80\n"
        : : "r"((uint32_t)c), "r"((uint32_t)color), "r"(coords) : "eax", "ebx", "ecx", "edx"
    );
}

#endif