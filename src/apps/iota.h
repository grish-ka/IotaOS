/* * IotaOS - iota.h
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

#endif