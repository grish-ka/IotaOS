/*
* IotaOS - wc.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>
#include <ctype.h>

static int show_lines, show_words, show_bytes;
static long total_lines, total_words, total_bytes;

static void report(long l, long w, long b, const char *name) {
    if (show_lines) printf("%7ld ", l);
    if (show_words) printf("%7ld ", w);
    if (show_bytes) printf("%7ld ", b);
    if (name) printf("%s", name);
    putchar('\n');
}

static void wc_fd(int fd, const char *name) {
    char buf[4096];
    long l = 0, w = 0, b = 0;
    int in_word = 0;
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        b += n;
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') l++;
            if (isspace((unsigned char)buf[i])) in_word = 0;
            else if (!in_word) {
                in_word = 1;
                w++;
            }
        }
    }
    total_lines += l;
    total_words += w;
    total_bytes += b;
    report(l, w, b, name);
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int i = 1, status = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'l') show_lines = 1;
            else if (*p == 'w') show_words = 1;
            else if (*p == 'c' || *p == 'm') show_bytes = 1;
        }
    }
    if (!show_lines && !show_words && !show_bytes) show_lines = show_words = show_bytes = 1;

    if (i >= argc) {
        wc_fd(STDIN_FILENO, NULL);
        return 0;
    }
    int files = argc - i;
    for (; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            warn_errno(prog, argv[i]);
            status = 1;
            continue;
        }
        wc_fd(fd, argv[i]);
        close(fd);
    }
    if (files > 1) report(total_lines, total_words, total_bytes, "total");
    return status;
}
