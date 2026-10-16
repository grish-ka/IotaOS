/* 
 * IotaOS - tar.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef TAR_H
#define TAR_H

#include <stdint.h>
#include "fs/vfs.h"

typedef struct {
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];       
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
    char padding[12];    
} __attribute__((packed)) tar_header_t;

/* Unpack the ustar ramdisk into the VFS tree under "/"; returns the number of entries */
int tar_load(uint32_t initrd_address);

#endif