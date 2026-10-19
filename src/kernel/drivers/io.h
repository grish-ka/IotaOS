/* 
 * IotaOS - io.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef IO_H
#define IO_H

#include <stdint.h>

/* Reads one byte from the specified hardware port */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %w1, %b0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/* Writes one byte to the specified hardware port */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

/* Reads one 16-bit word from the specified hardware port */
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ( "inw %w1, %w0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/* Writes one 16-bit word to the specified hardware port */
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ( "outw %w0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

/* Reads count 16-bit words from port into buffer */
static inline void insw(uint16_t port, void* addr, uint32_t count) {
    __asm__ volatile ( "cld; rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory" );
}

/* Writes count 16-bit words from buffer to port */
static inline void outsw(uint16_t port, const void* addr, uint32_t count) {
    __asm__ volatile ( "cld; rep outsw" : "+S"(addr), "+c"(count) : "d"(port) : "memory" );
}

#endif /* IO_H */