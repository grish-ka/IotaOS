/*
 * IotaOS - spinner.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "iota.h"

int main(int argc, char* argv[]) {
    uint8_t color = 10; /* Default: Light Green */
    uint32_t speed = 100; /* Default: 100ms */

    /* Parse optional arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "red") == 0) color = 12;
        else if (strcmp(argv[1], "blue") == 0) color = 9;
        else if (strcmp(argv[1], "yellow") == 0) color = 14;
        else if (strcmp(argv[1], "white") == 0) color = 15;
    }

    if (argc > 2) {
        if (strcmp(argv[2], "fast") == 0) speed = 30;
        else if (strcmp(argv[2], "slow") == 0) speed = 300;
    }

    /* Daemonize the process */
    int pid = iota_fork();

    if (pid < 0) {
        printf("spinner: Failed to launch background task.\n");
        return 1;
    }

    if (pid > 0) {
        /* Parent executes this block and immediately returns to IOSH */
        printf("Spinner daemonized in background (PID %d).\n", pid);
        return 0; 
    }

    /* Child executes this block infinitely */
    const char spinner_chars[] = {'|', '/', '-', '\\'};
    int i = 0;

    while (1) {
        iota_putchar_at(spinner_chars[i], color, 79, 0);
        i = (i + 1) % 4;
        iota_sleep(speed);
    }

    return 0;
}