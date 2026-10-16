/*
* IotaOS - more.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>
#include <termios.h>

#define PAGE_LINES 24
#define COLS 80

static int keys_fd = -1;
static struct termios saved;

/* Wait for a key from the terminal (stdin may be a pipe) */
static int get_key(void) {
    unsigned char c;
    if (read(keys_fd, &c, 1) != 1) return 'q';
    return c;
}

static int show(FILE *in, const char *name) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    int shown = 0, page = PAGE_LINES - 1;
    long total = 0, bytes = 0;

    while ((n = getline(&line, &cap, in)) > 0) {
        bytes += n;
        fputs(line, stdout);
        total++;
        /* Long lines take more than one screen row */
        shown += (n > COLS) ? (int)((n - 1) / COLS) + 1 : 1;
        if (keys_fd < 0 || shown < page) continue;

        printf("\033[7m--More--%s%s\033[0m", name ? " " : "", name ? name : "");
        fflush(stdout);
        int k = get_key();
        printf("\r\033[K");
        if (k == 'q' || k == 'Q' || k == 0x03) {
            free(line);
            return 1;
        }
        shown = 0;
        page = (k == '\n' || k == '\r') ? 1 : PAGE_LINES - 1;
    }
    free(line);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    if (isatty(STDOUT_FILENO)) {
        keys_fd = open("/dev/tty", O_RDONLY);
        if (keys_fd >= 0) {
            tcgetattr(keys_fd, &saved);
            struct termios raw = saved;
            raw.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(keys_fd, TCSANOW, &raw);
        }
    }

    int status = 0;
    if (argc < 2) {
        show(stdin, NULL);
    } else {
        for (int i = 1; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (!f) {
                warn_errno(prog, argv[i]);
                status = 1;
                continue;
            }
            if (argc > 2) printf("::::::::::::::\n%s\n::::::::::::::\n", argv[i]);
            int quit = show(f, argc > 2 ? argv[i] : NULL);
            fclose(f);
            if (quit) break;
        }
    }

    if (keys_fd >= 0) tcsetattr(keys_fd, TCSANOW, &saved);
    return status;
}
