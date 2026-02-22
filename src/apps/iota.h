/* * IotaOS - iota.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */


#ifndef IOTA_H
#define IOTA_H

#include <stdint.h>

/* Syscall Wrappers */

static inline void iota_print(const char* str) {
    uint32_t real_addr;
    __asm__ volatile (
        "call 1f\n"         /* Push EIP to stack */
        "1: pop %%eax\n"    /* Pop EIP into EAX */
        "sub $(1b), %%eax\n" /* Subtract the compiled offset of '1b' */
        "add %1, %%eax\n"   /* Add the offset of our string */
        "mov %%eax, %%ebx\n"
        "mov $1, %%eax\n"
        "int $0x80"
        : "=a"(real_addr) 
        : "r"(str) 
        : "ebx"
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

#endif