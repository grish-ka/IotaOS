/*
 * IotaOS - meminfo.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "iota.h"

int main() {
    int pid = iota_fork();

    if (pid < 0) {
        printf("meminfo: fork() failed.\n");
        return 1;
    }

    if (pid > 0) {
        /* Parent process exits immediately, returning you to the shell */
        printf("Started Live Memory Monitor in background (PID %d).\n", pid);
        return 0;
    }

    /* Child process loops forever to update the screen */
    while(1) {
        uint32_t free_blocks, total_blocks;
        iota_meminfo(&free_blocks, &total_blocks);
        
        uint32_t free_mb = (free_blocks * 4096) / 1024 / 1024;
        uint32_t total_mb = (total_blocks * 4096) / 1024 / 1024;
        uint32_t used_mb = total_mb - free_mb;

        char buf[64];
        sprintf(buf, " Mem: %d / %d MB | Free: %d MB ", used_mb, total_mb, free_mb);

        uint32_t x = 79 - strlen(buf);
        
        for (int j = 0; buf[j]; j++) {
            iota_putchar_at(buf[j], 11, x++, 24); 
        }

        iota_sleep(1000); 
    }
}