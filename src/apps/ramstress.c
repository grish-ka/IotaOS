/*
 * IotaOS - ramstress.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "iota.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ramstress <MB>\n");
        return 1;
    }

    uint32_t mb = atoi(argv[1]);
    if (mb == 0) return 0;

    printf("--- RAM Stress Test ---\n");
    printf("Allocating %d MB of RAM...\n", mb);
    iota_stress_ram(1, mb);

    printf("Done! Background 'meminfo' should now drop.\n");
    printf("Type any key and press ENTER to abort/free...\n");
    
    char buf[32];
    memset(buf, 0, 32);
    gets(buf, 32);

    printf("Freeing RAM and exiting...\n");
    iota_stress_ram(0, 0);
    
    return 0;
}