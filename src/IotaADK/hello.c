/*
* IotaOS - hello.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "iota.h"

void main() {
    /* Remember the Golden Rule: Wrap string literals in IOTA_RELOC! */
    iota_print_str(IOTA_RELOC("Hello from the IotaOS ADK!\n"));
    
    char name[32];
    for(int i=0; i<32; i++) name[i] = 0;
    
    iota_print_str(IOTA_RELOC("Type something: "));
    iota_get_line(name, 32);
    
    iota_print_str(IOTA_RELOC("You typed: "));
    iota_print_str(name);
    iota_print_str(IOTA_RELOC("\n"));

}