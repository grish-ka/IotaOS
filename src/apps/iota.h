/*
* IotaOS - iota.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef IOTA_H
#define IOTA_H

#include <stdint.h>

/**
 * IOTA_RELOC: Calculates the base offset at RUNTIME.
 * The &(ptr)[0] trick forces the compiler to treat arrays as pointers,
 * fixing the "cast specifies array type" error.
 */
#define IOTA_RELOC(ptr) ({ \
    uintptr_t __base; \
    __asm__ volatile ( \
        "call 1f\n" \
        "1: pop %0\n"      /* Get current EIP */ \
        "sub $1b, %0\n"    /* Subtract link-time address of label '1' */ \
        : "=r"(__base) \
    ); \
    (typeof(&(ptr)[0]))((uintptr_t)(ptr) + __base); \
})

/* --- Syscall Wrappers --- */

static inline void iota_print(const char* str, uint32_t len) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a"(1), "b"(1), "c"(str), "d"(len)
        : "memory"
    );
}

static inline void iota_get_line(char* buf, int size) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a"(3), "b"(buf), "c"(size)
        : "memory"
    );
}

static inline void iota_clear() {
    __asm__ volatile (
        "int $0x80" 
        : 
        : "a"(4) 
        : "memory"
    );
}

static inline int iota_strcmp(const char* str1, const char* str2) {
    while (*str1 != '\0' && *str2 != '\0') {
        if (*str1 != *str2) {
            return 1; /* They don't match! */
        }
        str1++;
        str2++;
    }
    /* If both reached the end at the same time, they match */
    if (*str1 == '\0' && *str2 == '\0') {
        return 0; 
    }
    return 1;
}

static inline uint32_t iota_strlen(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}

static inline void iota_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0'; /* Null-terminate the destination string */
}
#endif