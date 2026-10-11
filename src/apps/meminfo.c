/*
 * IotaOS - meminfo.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "iota.h"

/* Helper function to draw a number at a specific X/Y coordinate */
void draw_num_at(uint32_t num, uint8_t color, uint32_t *x_pos, uint32_t y) {
    if (num == 0) { 
        iota_putchar_at('0', color, *x_pos, y);
        (*x_pos)++;
        return; 
    }
    
    char buf[16];
    int i = 14;
    buf[15] = '\0';
    
    while(num > 0) {
        buf[i--] = '0' + (num % 10);
        num /= 10;
    }
    
    char *str = &buf[i+1];
    int j = 0;
    while(str[j]) {
        iota_putchar_at(str[j], color, *x_pos, y);
        (*x_pos)++;
        j++;
    }
}

void main() {
    iota_print_str("Started Live Memory Monitor! (Bottom Right)\n");
    
    while(1) {
        uint32_t free_blocks, total_blocks;
        iota_meminfo(&free_blocks, &total_blocks);
        
        uint32_t free_mb = (free_blocks * 4096) / 1024 / 1024;
        uint32_t total_mb = (total_blocks * 4096) / 1024 / 1024;
        uint32_t used_mb = total_mb - free_mb;

        /* Start drawing at column 45 so the full text ends 3 columns before the right edge */
        uint32_t x = 45;
        uint8_t color = 11; /* Light Cyan */

        /* 1. Draw " Mem: " */
        const char *prefix = " Mem: ";
        for (int j = 0; prefix[j]; j++) iota_putchar_at(prefix[j], color, x++, 24);
        
        /* 2. Draw the used memory number */
        draw_num_at(used_mb, color, &x, 24);
        
        /* 3. Draw " / " */
        const char *divider = " / ";
        for (int j = 0; divider[j]; j++) iota_putchar_at(divider[j], color, x++, 24);
        
        /* 4. Draw the total memory number */
        draw_num_at(total_mb, color, &x, 24);
        
        /* 5. Draw " MB " */
        const char *suffix = " MB ";
        for (int j = 0; suffix[j]; j++) iota_putchar_at(suffix[j], color, x++, 24);

        const char *free_str = " Free: ";
        for (int j = 0; free_str[j]; j++) iota_putchar_at(free_str[j], color, x++, 24);
        draw_num_at(free_mb, color, &x, 24);
        const char *mb_str = " MB";
        for (int j = 0; mb_str[j]; j++) iota_putchar_at(mb_str[j], color, x++, 24);

        /* 6. Clear out any leftover characters */
        while (x < 77) {
            iota_putchar_at(' ', color, x++, 24);
        }

        /* Update once every second */
        iota_sleep(1000); 
    }
}