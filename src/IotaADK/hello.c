/*
* IotaOS - hello.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "iota.h"

void main() {
    iota_print_str("Hello from the IotaOS ADK!\n");
    
    char name[32];
    for(int i=0; i<32; i++) name[i] = 0;
    
    iota_print_str("Type something: ");
    iota_get_line(name, 32);
    
    iota_print_str("You typed: ");
    iota_print_str(name);
    iota_print_str("\n");

}