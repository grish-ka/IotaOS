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

/* Mounts the ramdisk into the VFS tree and returns the root node */
vfs_node_t* tar_mount(uint32_t initrd_address);

/* Legacy function kept for the moment until syscalls are updated */
void* tar_get_file(uint32_t initrd_address, const char* target_filename, uint32_t *file_size);

#endif