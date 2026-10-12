/*
 * IotaOS ADK - iota.h
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

static inline int iota_read(int fd, void* buf, uint32_t count) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(3), "b"(fd), "c"(buf), "d"(count) : "memory");
    return ret;
}

static inline int iota_write(int fd, const void* buf, uint32_t count) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(4), "b"(fd), "c"(buf), "d"(count) : "memory");
    return ret;
}

static inline int iota_open(const char* filename) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(16), "b"(filename) : "memory");
    return ret;
}

static inline int iota_close(int fd) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(17), "b"(fd) : "memory");
    return ret;
}

static inline void iota_ls(const char* target) {
    __asm__ volatile ("int $0x80" : : "a"(8), "b"(target) : "memory");
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

static inline void iota_clear() {
    __asm__ volatile ("int $0x80" : : "a"(14) : "memory");
}

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

static inline void* iota_sbrk(int increment) {
    void* ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(15), "b"(increment) : "memory");
    return ret;
}

static inline void iota_get_args(char* buf) {
    __asm__ volatile ("int $0x80" : : "a"(18), "b"(buf) : "memory");
}

static inline int iota_fork(void) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(19) : "memory");
    return ret;
}

static inline void iota_wait(int pid) {
    __asm__ volatile ("int $0x80" : : "a"(20), "b"(pid) : "memory");
}

static inline void iota_get_env(char* buf) {
    __asm__ volatile ("int $0x80" : : "a"(21), "b"(buf) : "memory");
}

static inline int iota_chdir(const char* path) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(22), "b"(path) : "memory");
    return ret;
}

static inline void iota_getcwd(char* buf) {
    __asm__ volatile ("int $0x80" : : "a"(23), "b"(buf) : "memory");
}

#endif