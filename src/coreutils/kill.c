/*
* IotaOS - kill.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <signal.h>

static const char *const names[] = {
    NULL, "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "BUS", "FPE", "KILL", "USR1",
    "SEGV", "USR2", "PIPE", "ALRM", "TERM",
};
#define NSIGNAMES (int)(sizeof(names) / sizeof(names[0]))

static int parse_signal(const char *s) {
    if (*s >= '0' && *s <= '9') return atoi(s);
    if (!strncmp(s, "SIG", 3)) s += 3;
    for (int i = 1; i < NSIGNAMES; i++) if (!strcmp(s, names[i])) return i;
    return -1;
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int sig = SIGTERM, i = 1;

    if (i < argc && !strcmp(argv[i], "-l")) {
        for (int k = 1; k < NSIGNAMES; k++) printf("%2d) SIG%-8s%s", k, names[k], k % 5 == 0 ? "\n" : "");
        putchar('\n');
        return 0;
    }
    if (i < argc && argv[i][0] == '-' && argv[i][1]) {
        const char *s = argv[i] + 1;
        if (!strcmp(argv[i], "-s") && i + 1 < argc) s = argv[++i];
        sig = parse_signal(s);
        if (sig < 0) {
            fprintf(stderr, "%s: unknown signal '%s'\n", prog, s);
            return 1;
        }
        i++;
    }
    if (i >= argc) {
        fprintf(stderr, "Usage: %s [-s SIGNAL | -SIGNAL] PID...\n", prog);
        return 2;
    }
    int status = 0;
    for (; i < argc; i++) {
        if (kill(atoi(argv[i]), sig) < 0) {
            fprintf(stderr, "%s: (%s) - %s\n", prog, argv[i], strerror(errno));
            status = 1;
        }
    }
    return status;
}
