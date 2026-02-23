/* 
 * IotaOS - iota.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef IOTA_H
#define IOTA_H

#include <stdint.h>

/* Syscall Wrappers */

static inline void iota_print(const char* str, uint32_t len) {
    __asm__ volatile (
        "call 1f\n"
        "1: pop %%eax\n"            /* Get current EIP */
        "sub $(1b), %%eax\n"        /* Subtract link-time address of '1' */
        "add %0, %%eax\n"           /* Add the pointer 'str' */
        "mov %%eax, %%ecx\n"        /* ECX now has the ACTUAL address in RAM */
        "mov $1, %%eax\n"           /* Syscall 1 */
        "mov $1, %%ebx\n"           /* FD 1 */
        "int $0x80"
        : 
        : "r"(str), "r"(len)
        : "eax", "ebx", "ecx", "edx"
    );
}
static inline void iota_reboot() {
    __asm__ volatile (
        "mov $2, %eax\n"
        "int $0x80"
    );
}

static inline void iota_panic(const char* msg) {
    __asm__ volatile (
        "mov $0, %eax\n"
        "mov %0, %ebx\n"
        "int $0x80"
        : : "r"(msg) : "eax", "ebx"
    );
}

static inline int iota_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static inline void iota_get_line(char* buf, int size) {
    __asm__ volatile (
        "mov $3, %%eax\n"  /* Must be 3 for SYS_READ_LINE */
        "mov %0, %%ebx\n"
        "mov %1, %%ecx\n"
        "int $0x80"
        : : "r"(buf), "r"(size) : "eax", "ebx", "ecx"
    );
}

static inline void iota_clear() {
    __asm__ volatile ("mov $4, %eax; int $0x80"); /* Must be 4 */
}

#endif