/* * IotaOS - tar.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "fs/tar.h"
#include "drivers/stdio.h"
#include "drivers/string.h"

/* Helper: TAR stores sizes as base-8 (octal) strings. We need a real integer. */
static uint32_t octal_string_to_int(const char *str, int size) {
    uint32_t result = 0;
    for (int i = 0; i < size; i++) {
        if (str[i] >= '0' && str[i] <= '7') {
            result = result * 8 + (str[i] - '0');
        }
    }
    return result;
}

void tar_parse(uint32_t initrd_address) {
    uint32_t current_address = initrd_address;
    
    printf("--- Mounting IotaOS Ramdisk (TAR) ---\n");

    while (1) {
        tar_header_t* header = (tar_header_t*)current_address;

        /* If the filename is empty, we hit the end of the archive */
        if (header->filename[0] == '\0') {
            break;
        }

        uint32_t file_size = octal_string_to_int(header->size, 11);
        printf("Found file: %s (%d bytes)\n", header->filename, file_size);

        /* * Move to the next file! 
         * 1. Jump over the 512-byte header.
         * 2. Jump over the file data itself.
         * 3. TAR pads file data to 512-byte blocks, so we add padding math.
         */
        current_address += 512; 
        current_address += ((file_size + 511) / 512) * 512;
    }
    
    printf("--- Ramdisk Mounted! ---\n\n");
}

/* Add this to the bottom of src/kernel/fs/tar.c */
void* tar_get_file(uint32_t initrd_address, const char* target_filename) {
    uint32_t current_address = initrd_address;
    while (1) {
        tar_header_t* header = (tar_header_t*)current_address;
        if (header->filename[0] == '\0') break;

        /* Extract size using that octal helper you already wrote */
        uint32_t file_size = octal_string_to_int(header->size, 11);

        if (strcmp(header->filename, target_filename) == 0) {
            return (void*)(current_address + 512); /* Return data after header */
        }

        current_address += 512 + (((file_size + 511) / 512) * 512);
    }
    return (void*)0;
}