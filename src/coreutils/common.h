/*
* IotaOS - common.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef COREUTILS_COMMON_H
#define COREUTILS_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* argv[0] without its directory, for messages like "ls: cannot access ..." */
static inline const char *prog_name(const char *argv0) {
    const char *slash = strrchr(argv0, '/');
    return slash ? slash + 1 : argv0;
}

/* Print "prog: what: strerror(errno)" to stderr */
static inline void warn_errno(const char *prog, const char *what) {
    fprintf(stderr, "%s: %s: %s\n", prog, what, strerror(errno));
}

/* Copy everything from fd in to fd out; returns 0 or -1 */
static inline int copy_fd(int in, int out) {
    char buf[4096];
    ssize_t n;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(out, buf + off, n - off);
            if (w < 0) return -1;
            off += w;
        }
    }
    return n < 0 ? -1 : 0;
}

/* Human-readable size like GNU -h: 1023, 1.0K, 4.5M */
static inline void human_size(unsigned long bytes, char *out, size_t size) {
    const char *units = "BKMGT";
    double v = bytes;
    int u = 0;
    while (v >= 1024 && u < 4) {
        v /= 1024;
        u++;
    }
    if (u == 0) snprintf(out, size, "%lu", bytes);
    else if (v < 10) snprintf(out, size, "%.1f%c", v, units[u]);
    else snprintf(out, size, "%.0f%c", v, units[u]);
}

/* Join dir and name with exactly one slash */
static inline void path_join(char *out, size_t size, const char *dir, const char *name) {
    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/') snprintf(out, size, "%s%s", dir, name);
    else snprintf(out, size, "%s/%s", dir, name);
}

#endif
