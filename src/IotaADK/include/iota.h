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

/* System call numbers; must match the kernel's cpu/syscall.h */
#define IOTA_SYS_READ      3
#define IOTA_SYS_WRITE     4
#define IOTA_SYS_EXIT      6
#define IOTA_SYS_SLEEP     9
#define IOTA_SYS_OPEN      16
#define IOTA_SYS_CLOSE     17
#define IOTA_SYS_CHDIR     22
#define IOTA_SYS_GETCWD    23
#define IOTA_SYS_IOCTL     24
#define IOTA_SYS_EXECVE    25
#define IOTA_SYS_WAITPID   26
#define IOTA_SYS_STAT      29
#define IOTA_SYS_LSEEK     30
#define IOTA_SYS_DUP       31
#define IOTA_SYS_DUP2      32
#define IOTA_SYS_PIPE      33
#define IOTA_SYS_FSTAT     34
#define IOTA_SYS_MKDIR     35
#define IOTA_SYS_UNLINK    36
#define IOTA_SYS_RMDIR     37
#define IOTA_SYS_RENAME    38
#define IOTA_SYS_GETDENTS  39
#define IOTA_SYS_FCNTL     40
#define IOTA_SYS_TIME      41
#define IOTA_SYS_PROCLIST  42
#define IOTA_SYS_KILL      43
#define IOTA_SYS_FTRUNCATE 44
#define IOTA_SYS_SYSINFO   45
#define IOTA_SYS_CHMOD     46
#define IOTA_SYS_UTIME     47
#define IOTA_SYS_POWEROFF  48
#define IOTA_SYS_GETIDS    49
#define IOTA_SYS_SETIDS    50
#define IOTA_SYS_CHOWN     51

/* Generic call: returns the kernel's result (negative errno on failure for
   the POSIX-style calls) */
static inline int iota_syscall3(int num, uint32_t a, uint32_t b, uint32_t c) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a), "c"(b), "d"(c) : "memory");
    return ret;
}

/* Kernel stat record (SYS_STAT / SYS_FSTAT) */
typedef struct {
    uint32_t ino;
    uint32_t mode;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t mtime;
    uint32_t rdev;
} iota_stat_t;

/* Directory entry record (SYS_GETDENTS) */
typedef struct {
    uint32_t ino;
    uint8_t type;   /* 1 file, 2 directory, 3 character device */
    char name[64];
} iota_dirent_t;

/* Process list record (SYS_PROCLIST) */
typedef struct {
    int pid;
    int ppid;
    char state;
    uint32_t start_tick;
    char name[32];
    uint32_t cpu_ticks;
    uint32_t uid;
} iota_proc_t;

/* SYS_SYSINFO */
typedef struct {
    uint32_t uptime_ticks;
    uint32_t ticks_per_second;
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t procs;
} iota_sysinfo_t;

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

/* Open read-only */
static inline int iota_open(const char* filename) {
    return iota_syscall3(IOTA_SYS_OPEN, (uint32_t)filename, 0, 0);
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

/* Replace this process. args is the space-separated argv; env is a
   NUL-separated list ending in an empty string, or NULL to keep the current one */
static inline int iota_execve(const char* path, const char* args, const char* env) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(25), "b"(path), "c"(args), "d"(env) : "memory");
    return ret;
}

/* Collect an exited child (pid -1 = any). Returns its pid, 0 with nohang if
   none has exited yet, or -1 if there is no such child */
static inline int iota_waitpid(int pid, int* exit_code, int nohang) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(26), "b"(pid), "c"(exit_code), "d"(nohang) : "memory");
    return ret;
}

static inline int iota_getpid(void) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(27) : "memory");
    return ret;
}

static inline int iota_getppid(void) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(28) : "memory");
    return ret;
}

static inline int iota_stat(const char* path, iota_stat_t* st) {
    return iota_syscall3(IOTA_SYS_STAT, (uint32_t)path, (uint32_t)st, 0);
}

#endif
