/* 
 * IotaOS - tar.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "fs/tar.h"
#include "fs/vfs.h"
#include "drivers/stdio.h"
#include "drivers/string.h"

/* Pre-allocate space for 64 files in our ramdisk */
vfs_node_t initrd_nodes[64];
int initrd_file_count = 0;
struct dirent initrd_dirent;
vfs_node_t initrd_root_node;

static uint32_t octal_string_to_int(const char *str, int size) {
    uint32_t result = 0;
    for (int i = 0; i < size; i++) {
        if (str[i] >= '0' && str[i] <= '7') {
            result = result * 8 + (str[i] - '0');
        }
    }
    return result;
}

/* VFS Read Hook: Reads data from a TAR file node */
static uint32_t tar_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    tar_header_t *header = (tar_header_t *)node->impl;
    uint32_t file_size = node->size;
    
    if (offset >= file_size) return 0;
    if (offset + size > file_size) size = file_size - offset;
    
    uint8_t *file_data = (uint8_t *)((uint32_t)header + 512);
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = file_data[offset + i];
    }
    
    return size;
}

/* VFS ReadDir Hook: Returns a child file in this directory */
static struct dirent *tar_readdir(vfs_node_t *node, uint32_t index) {
    if (node == &initrd_root_node && index < initrd_file_count) {
        int i = 0;
        while (initrd_nodes[index].name[i] != '\0') {
            initrd_dirent.name[i] = initrd_nodes[index].name[i];
            i++;
        }
        initrd_dirent.name[i] = '\0';
        initrd_dirent.ino = index;
        return &initrd_dirent;
    }
    return NULL;
}

/* VFS FindDir Hook: Searches for a specific file by name */
static vfs_node_t *tar_finddir(vfs_node_t *node, char *name) {
    if (node == &initrd_root_node) {
        for (int i = 0; i < initrd_file_count; i++) {
            if (strcmp(name, initrd_nodes[i].name) == 0) {
                return &initrd_nodes[i];
            }
        }
    }
    return NULL;
}

vfs_node_t* tar_mount(uint32_t initrd_address) {
    uint32_t current_address = initrd_address;
    
    printf("--- Mounting IotaOS Ramdisk (VFS) ---\n");

    /* Set up the root directory node */
    initrd_root_node.name[0] = '/';
    initrd_root_node.name[1] = '\0';
    initrd_root_node.flags = FS_DIRECTORY;
    initrd_root_node.read = NULL;
    initrd_root_node.write = NULL;
    initrd_root_node.open = NULL;
    initrd_root_node.close = NULL;
    initrd_root_node.readdir = &tar_readdir;
    initrd_root_node.finddir = &tar_finddir;
    initrd_root_node.ptr = NULL;
    initrd_root_node.impl = 0;

    initrd_file_count = 0;

    while (1) {
        tar_header_t* header = (tar_header_t*)current_address;

        if (header->filename[0] == '\0' || initrd_file_count >= 64) {
            break;
        }

        uint32_t file_size = octal_string_to_int(header->size, 11);
        
        /* Register the file in the VFS tree */
        vfs_node_t *node = &initrd_nodes[initrd_file_count];
        int i = 0;
        while (header->filename[i] != '\0' && i < 127) {
            node->name[i] = header->filename[i];
            i++;
        }
        node->name[i] = '\0';
        node->flags = FS_FILE;
        node->size = file_size;
        node->impl = (uint32_t)header; /* Store pointer to header so we can find data later */
        node->read = &tar_read;
        node->write = NULL; /* Ramdisk is read-only */
        node->open = NULL;
        node->close = NULL;
        node->readdir = NULL;
        node->finddir = NULL;

        printf("VFS: Linked file /%s (%d bytes)\n", node->name, file_size);
        
        initrd_file_count++;
        current_address += 512 + (((file_size + 511) / 512) * 512);
    }
    
    printf("--- VFS Mount Complete! ---\n\n");
    return &initrd_root_node;
}

/* Legacy function kept for the moment until syscalls are fully updated */
void* tar_get_file(uint32_t initrd_address, const char* target_filename, uint32_t *file_size_out) {
    uint32_t current_address = initrd_address;
    while (1) {
        tar_header_t* header = (tar_header_t*)current_address;
        if (header->filename[0] == '\0') break;
        uint32_t file_size = octal_string_to_int(header->size, 11);

        int match = 1;
        int i = 0;
        while (target_filename[i] != '\0' || header->filename[i] != '\0') {
            if (target_filename[i] != header->filename[i]) { match = 0; break; }
            i++;
        }

        if (match) {
            if (file_size_out) *file_size_out = file_size;
            return (void*)(current_address + 512);
        }
        current_address += 512 + (((file_size + 511) / 512) * 512);
    }
    return (void*)0;
}