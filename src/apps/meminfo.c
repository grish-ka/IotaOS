/*
 * IotaOS - meminfo.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "iota.h"

void main() {
    printf("Started Live Memory Monitor! (Bottom Right)\n");
    
    while(1) {
        uint32_t free_blocks, total_blocks;
        iota_meminfo(&free_blocks, &total_blocks);
        
        uint32_t free_mb = (free_blocks * 4096) / 1024 / 1024;
        uint32_t total_mb = (total_blocks * 4096) / 1024 / 1024;
        uint32_t used_mb = total_mb - free_mb;

        /* Use standard sprintf to format the entire line at once! */
        char buf[64];
        sprintf(buf, " Mem: %d / %d MB | Free: %d MB ", used_mb, total_mb, free_mb);

        /* Dynamically calculate the starting X position based on string length */
        uint32_t x = 79 - strlen(buf);
        
        /* Draw the string character by character to Row 24 (Bottom of screen) */
        for (int j = 0; buf[j]; j++) {
            iota_putchar_at(buf[j], 11, x++, 24); /* 11 is Light Cyan */
        }

        /* Sleep for 1 second */
        iota_sleep(1000); 
    }
}