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

// Renamed to match your iosh.c calls
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

static inline void iota_exec(const char* filename) {
    __asm__ volatile (
        "mov %0, %%ebx\n"
        "mov $7, %%eax\n"
        "int $0x80"
        : : "r"(filename) : "eax", "ebx"
    );
}

static inline void iota_reboot() {
    __asm__ volatile ("mov $2, %%eax; int $0x80" ::: "eax");
}

static inline void iota_exit(int exit_code) {
    __asm__ volatile (
        "mov %0, %%ebx\n"    /* Put our exit_code into EBX */
        "mov $6, %%eax\n"    /* SYS_EXIT */
        "int $0x80"
        : : "r"(exit_code) : "eax", "ebx"
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

#endif