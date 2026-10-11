/*
 * IotaOS - ramstress.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

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

void main() {
    char buf[32];
    for(int i = 0; i < 32; i++) buf[i] = 0;

    iota_print_str(IOTA_RELOC("--- RAM Stress Test ---\n"));
    iota_print_str(IOTA_RELOC("Enter amount to allocate (MB): "));
    iota_get_line(buf, 32);

    uint32_t mb = atoi(buf);
    if (mb == 0) iota_exit(0);

    iota_print_str(IOTA_RELOC("Allocating RAM...\n"));
    iota_stress_ram(1, mb);

    iota_print_str(IOTA_RELOC("Done! Background 'meminfo' should now drop.\n"));
    iota_print_str(IOTA_RELOC("Type any key and press ENTER to abort/free...\n"));
    
    /* Wait for user input to abort */
    iota_get_line(buf, 32);

    iota_print_str(IOTA_RELOC("Freeing RAM and exiting...\n"));
    iota_stress_ram(0, 0);
    
    iota_exit(0);
}