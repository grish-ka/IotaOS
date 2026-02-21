/* * IotaOS - tar.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef TAR_H
#define TAR_H

#include <stdint.h>

/* The standard USTAR header is exactly 512 bytes long */
typedef struct {
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];       /* File size in ASCII octal! */
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];    /* Pads the struct to exactly 512 bytes */
} __attribute__((packed)) tar_header_t;

/* Parse the ramdisk and list files */
void tar_parse(uint32_t initrd_address);
void* tar_get_file(uint32_t initrd_address, const char* target_filename);

#endif