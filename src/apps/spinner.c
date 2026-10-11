/*
 * IotaOS - spinner.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include "iota.h"

int main() {
    printf("Spinner task started! (Look at top right corner)\n");
    
    const char spinner_chars[] = {'|', '/', '-', '\\'};
    int i = 0;
    
    while(1) {
        /* Draw the spinner in the top right corner (x=79, y=0) 
           Color 10 is Light Green */
        iota_putchar_at(spinner_chars[i], 10, 79, 0);
        
        i++;
        if (i >= 4) i = 0;
        
        /* Sleep for 100 milliseconds so it doesn't spin too fast */
        iota_sleep(100);
    }
    
    return 0;
}