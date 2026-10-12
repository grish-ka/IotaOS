/*
 * IotaOS - vfs.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "fs/vfs.h"
#include "drivers/string.h"

vfs_node_t *vfs_root = NULL; 

/* The master root directory that holds our mountpoints */
vfs_node_t root_nodes[16];
int root_node_count = 0;
vfs_node_t real_root_node;
struct dirent root_dirent;

static struct dirent *root_readdir(vfs_node_t *node, uint32_t index) {
    (void)node;
    if (index < (uint32_t)root_node_count) {
        int i = 0;
        while (root_nodes[index].name[i] != '\0') {
            root_dirent.name[i] = root_nodes[index].name[i];
            i++;
        }
        root_dirent.name[i] = '\0';
        root_dirent.ino = index;
        return &root_dirent;
    }
    return NULL;
}

static vfs_node_t *root_finddir(vfs_node_t *node, char *name) {
    (void)node;
    for (int i = 0; i < root_node_count; i++) {
        if (strcmp(name, root_nodes[i].name) == 0) {
            return &root_nodes[i];
        }
    }
    return NULL;
}

void vfs_init(void) {
    real_root_node.name[0] = '/';
    real_root_node.name[1] = '\0';
    real_root_node.flags = FS_DIRECTORY;
    real_root_node.read = NULL;
    real_root_node.write = NULL;
    real_root_node.open = NULL;
    real_root_node.close = NULL;
    real_root_node.readdir = &root_readdir;
    real_root_node.finddir = &root_finddir;
    real_root_node.ioctl = NULL;
    vfs_root = &real_root_node;
}

void vfs_mount(const char* path, vfs_node_t* local_root) {
    const char* name = path;
    if (name[0] == '/') name++; /* Strip leading slash */

    vfs_node_t* mount_node = &root_nodes[root_node_count++];
    int i = 0;
    while (name[i] != '\0') {
        mount_node->name[i] = name[i];
        i++;
    }
    mount_node->name[i] = '\0';
    mount_node->flags = FS_MOUNTPOINT | FS_DIRECTORY;
    mount_node->ptr = local_root; 
}

vfs_node_t* vfs_get_node(const char* path) {
    if (!vfs_root || !path) return NULL;
    
    vfs_node_t* current = vfs_root;
    char buffer[128];
    
    while (*path == '/') path++;
    if (*path == '\0') return current; 
    
    while (*path != '\0') {
        int i = 0;
        while (*path != '/' && *path != '\0') {
            buffer[i++] = *path++;
        }
        buffer[i] = '\0';
        
        if (current->flags & FS_MOUNTPOINT) {
            current = current->ptr;
        }
        
        current = vfs_finddir(current, buffer);
        if (!current) return NULL;
        
        while (*path == '/') path++;
    }
    
    if (current && (current->flags & FS_MOUNTPOINT)) {
        return current->ptr;
    }
    return current;
}

uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node && node->read) return node->read(node, offset, size, buffer);
    return 0;
}
uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node && node->write) return node->write(node, offset, size, buffer);
    return 0;
}
void vfs_open(vfs_node_t *node) { if (node && node->open) node->open(node); }
void vfs_close(vfs_node_t *node) { if (node && node->close) node->close(node); }
struct dirent *vfs_readdir(vfs_node_t *node, uint32_t index) {
    if ((node->flags & 0x07) == FS_DIRECTORY && node->readdir) return node->readdir(node, index);
    return NULL;
}
vfs_node_t *vfs_finddir(vfs_node_t *node, char *name) {
    if ((node->flags & 0x07) == FS_DIRECTORY && node->finddir) return node->finddir(node, name);
    return NULL;
}
int vfs_ioctl(vfs_node_t *node, uint32_t request, void *arg) {
    if (node && node->ioctl) return node->ioctl(node, request, arg);
    return -1;
}