/*
* IotaOS - bsod.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iota.h"

static void raise_exception(int vector) {
    switch (vector) {
    case 0: {
        volatile int zero = 0;
        volatile int x = 1 / zero;
        (void)x;
        break;
    }
    case 1:
        /* Using standard extended asm because INT 1 does not jump to a C label.
           volatile prevents the compiler from optimizing this away. */
        __asm__ volatile ("int $1");
        break;
    case 3:
        __asm__ volatile ("int3");
        break;
    case 6:
        __asm__ volatile ("ud2");
        break;
    case 13:
        /* Load a segment selector that is not in the GDT */
        __asm__ volatile ("mov $0x1234, %%ax\n mov %%ax, %%ds" : : : "eax");
        break;
    case 14:
        /* Above the identity-mapped RAM and below the heap: never mapped */
        *(volatile int *)0xE0000000 = 1;
        break;
    default:
        fprintf(stderr, "bsod: no way to raise exception %d (try 0, 1, 3, 6, 13 or 14)\n", vector);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc > 2 && strcmp(argv[1], "-x") == 0) {
        raise_exception(atoi(argv[2]));
        return 1;
    }

    uint32_t code = 0xDEADBEEF;
    char message[80] = "Manually initiated crash";

    if (argc > 1) {
        char *end;
        code = strtoul(argv[1], &end, 16); /* "13B" and "0x13B" both work */
        if (*end != '\0') {
            fprintf(stderr, "bsod: '%s' is not a hex error code\n", argv[1]);
            return 1;
        }
    }
    if (argc > 2) {
        message[0] = '\0';
        for (int i = 2; i < argc; i++) {
            if (i > 2) strncat(message, " ", sizeof(message) - strlen(message) - 1);
            strncat(message, argv[i], sizeof(message) - strlen(message) - 1);
        }
    }

    iota_panic(message, code);
    return 0;
}
