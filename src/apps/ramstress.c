/*
 * IotaOS - ramstress.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "iota.h"

/* Helper to convert a string to an integer */
int atoi(const char* str) {
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

int main() {
    char buf[32];
    memset(buf, 0, 32);

    printf("--- RAM Stress Test ---\n");
    printf("Enter amount to allocate (MB): ");
    gets(buf, 32);

    uint32_t mb = atoi(buf);
    if (mb == 0) return 0;

    printf("Allocating RAM...\n");
    iota_stress_ram(1, mb);

    printf("Done! Background 'meminfo' should now drop.\n");
    printf("Type any key and press ENTER to abort/free...\n");
    
    /* Wait for user input to abort */
    memset(buf, 0, 32);
    gets(buf, 32);

    printf("Freeing RAM and exiting...\n");
    iota_stress_ram(0, 0);
    
    return 0;
}