/*
* IotaOS - echo.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

/* echo [-neE] [STRING]... */
int main(int argc, char *argv[]) {
    int newline = 1, escapes = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char *p = argv[i] + 1;
        if (strspn(p, "neE") != strlen(p)) break;
        for (; *p; p++) {
            if (*p == 'n') newline = 0;
            else if (*p == 'e') escapes = 1;
            else escapes = 0;
        }
    }
    for (; i < argc; i++) {
        for (const char *p = argv[i]; *p; p++) {
            if (escapes && *p == '\\' && p[1]) {
                p++;
                switch (*p) {
                case 'n': putchar('\n'); break;
                case 't': putchar('\t'); break;
                case 'r': putchar('\r'); break;
                case 'a': putchar('\a'); break;
                case 'b': putchar('\b'); break;
                case 'e': putchar('\033'); break;
                case '\\': putchar('\\'); break;
                case 'c': return 0;
                default: putchar('\\'); putchar(*p); break;
                }
            } else {
                putchar(*p);
            }
        }
        if (i + 1 < argc) putchar(' ');
    }
    if (newline) putchar('\n');
    return 0;
}
