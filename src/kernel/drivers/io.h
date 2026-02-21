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

#endif /* IO_H */