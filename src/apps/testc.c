/*
* IotaOS - testc.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "iota.h"

void main() {
    /* Uses the new auto-length print function and relocates the string! */
    iota_print_str(IOTA_RELOC("IotaOS: C App is running perfectly!\n"));
    
    /* We should also exit gracefully so it returns to the shell */
    iota_exit(); 
}