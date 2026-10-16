/*
* IotaOS - sleep.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

/* sleep NUMBER[smhd]... (fractions allowed) */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "%s: missing operand\n", prog_name(argv[0]));
        return 1;
    }
    double total = 0;
    for (int i = 1; i < argc; i++) {
        char *end;
        double v = strtod(argv[i], &end);
        if (*end == 'm') v *= 60;
        else if (*end == 'h') v *= 3600;
        else if (*end == 'd') v *= 86400;
        total += v;
    }
    unsigned long ms = (unsigned long)(total * 1000);
    while (ms > 0) {
        unsigned long chunk = ms > 1000 ? 1000 : ms;
        usleep(chunk * 1000);
        ms -= chunk;
    }
    return 0;
}
