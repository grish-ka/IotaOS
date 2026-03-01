/*
* IotaOS - iosh.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

// DONE: If statements not working in iosh.c

#include "iota.h"

void main() {
    // Note: We use IOTA_RELOC for the string literal
    iota_print(IOTA_RELOC("\nWelcome to Iota Shell (IOSH) v0.1.0!\n"), 38);
    
    char cmd[256];
    // Zero the buffer
    // for(int i = 0; i < 256; i++) cmd[i] = 0;

    while (1) {
        iota_print(IOTA_RELOC("user@IotaOS$ "), 13);
        iota_get_line(cmd, 256);
        
        iota_print(cmd, iota_strlen(cmd));
        iota_print(IOTA_RELOC("\n"), 1);

        if (iota_strcmp(cmd, IOTA_RELOC("help")) == 0) {
            iota_print(IOTA_RELOC("Available: help, ls, version, clear\n"), 36);
        } 
        else if (iota_strcmp(cmd, IOTA_RELOC("ls")) == 0) {
            iota_print(IOTA_RELOC("bin/  dev/  iosh.ib  test.ib\n"), 29);
        }
        else if (iota_strcmp(cmd, IOTA_RELOC("clear")) == 0) {
            iota_clear();
        }
        else if (iota_strcmp(cmd, IOTA_RELOC("version")) == 0) {
            iota_print(IOTA_RELOC("IotaOS v0.1.0 (Stable)\n"), 23);
        } 
        else if (cmd[0] == '\0') {
            continue;
        }
        else {
            iota_print(IOTA_RELOC("Unknown command: "), 17);
            iota_print(cmd, iota_strlen(cmd));
            iota_print(IOTA_RELOC("\n"), 1);
        }
        iota_print(IOTA_RELOC("I made it here\n"), 17);
    }
}