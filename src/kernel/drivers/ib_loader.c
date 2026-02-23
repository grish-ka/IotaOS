/* 
 * IotaOS - ib_loader.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "ib_loader.h"
#include "stdio.h"
#include <stddef.h> /* <--- ADD THIS TO FIX THE 'NULL' ERROR */

void ib_load_and_run(void* file_data) {
    if (file_data == NULL) return;

    struct ib_header* header = (struct ib_header*)file_data;

    /* 1. Security Check: Is this actually a .ib file? */
    if (header->magic != IB_MAGIC) {
        printf("ib_loader: Invalid Magic! (0x%x)\n", header->magic);
        return;
    }

    /* 2. Calculate the jumping point */
    uint32_t code_ptr = (uint32_t)file_data + header->code_offset;

    /* 3. The "Jump": Turn that memory address into a function and call it */
    void (*app_entry)() = (void (*)())code_ptr;
    
    printf("ib_loader: Launching program...\n");
    
    app_entry(); 
    
    printf("\nib_loader: Program returned to kernel.\n");
}