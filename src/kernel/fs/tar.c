/* 
 * IotaOS - tar.c
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
        serial_printf("Found file: %s (%d bytes)\n", header->filename, file_size);

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
void* tar_get_file(uint32_t initrd_address, const char* target_filename,
                   uint32_t *file_size_out) {
    uint32_t current_address = initrd_address;
    
    serial_printf("Searching for target: '%s' at address 0x%x\n", target_filename, initrd_address);

    while (1) {
        tar_header_t* header = (tar_header_t*)current_address;
        
        if (header->filename[0] == '\0') {
            serial_printf(" -> BREAK: Hit a null byte at address 0x%x!\n", current_address);
            break;
        }

        uint32_t file_size = octal_string_to_int(header->size, 11);
        serial_printf(" -> Checking file: '%s' (Size: %d)\n", header->filename, file_size);

        /* Manual character-by-character check */
        int match = 1;
        int i = 0;
        while (target_filename[i] != '\0' || header->filename[i] != '\0') {
            if (target_filename[i] != header->filename[i]) {
                serial_printf("    Mismatch at char %d: expected '%c', got '%c'\n", 
                       i, target_filename[i], header->filename[i]);
                match = 0;
                break;
            }
            i++;
        }

        if (match) {
            serial_printf(" -> MATCH FOUND! Loading %s...\n", target_filename);
            if (file_size_out) *file_size_out = file_size;
            return (void*)(current_address + 512);
        }

        current_address += 512 + (((file_size + 511) / 512) * 512);
    }
    
    serial_printf(" -> ERROR: File not found in archive.\n");
    return (void*)0;
}