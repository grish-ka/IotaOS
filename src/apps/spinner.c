/*
 * IotaOS - spinner.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "iota.h"

void main() {
    iota_print_str(IOTA_RELOC("Running Spinner Task! Look at the top right!\n"));
    
    char spinner_chars[] = {'-', '\\', '|', '/'};
    int i = 0;
    
    while(1) {
        /* Draw the character at X: 79, Y: 0 (Top right corner), Color 10 (Light Green) */
        iota_putchar_at(spinner_chars[i], 10, 79, 0);
        
        i = (i + 1) % 4;
        
        /* Yield to the kernel and sleep for 100 milliseconds! */
        iota_sleep(100); 
    }
}