/*
 * IotaOS - vfs.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_PIPE        0x05
#define FS_SYMLINK     0x06
#define FS_MOUNTPOINT  0x08

struct vfs_node;

/* Function pointer definitions for filesystem drivers */
typedef uint32_t (*read_type_t)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
typedef void (*open_type_t)(struct vfs_node*);
typedef void (*close_type_t)(struct vfs_node*);
typedef struct dirent * (*readdir_type_t)(struct vfs_node*, uint32_t);
typedef struct vfs_node * (*finddir_type_t)(struct vfs_node*, char *name);
typedef int (*ioctl_type_t)(struct vfs_node*, uint32_t, void*); /* NEW: IOCTL */

typedef struct vfs_node {
    char name[128];         /* The requested filename */
    uint32_t mask;          /* Permissions mask */
    uint32_t uid;           /* Owning user */
    uint32_t gid;           /* Owning group */
    uint32_t flags;         /* Node type (File, Directory, Device) */
    uint32_t inode;         /* Device-specific ID */
    uint32_t size;          /* Size of the file in bytes */
    uint32_t impl;          /* Driver-specific tracking number */
    
    read_type_t read;       /* Read function */
    write_type_t write;     /* Write function */
    open_type_t open;       /* Open function */
    close_type_t close;     /* Close function */
    readdir_type_t readdir; /* Return a child directory entry */
    finddir_type_t finddir; /* Find a child node by name */
    ioctl_type_t ioctl;     /* IOCTL function */
    
    struct vfs_node *ptr;   /* Used by mountpoints and symlinks */
} vfs_node_t;

/* A standard POSIX directory entry */
struct dirent {
    char name[128];
    uint32_t ino;
};

/* The root of the entire filesystem (i.e. "/") */
extern vfs_node_t *vfs_root;

void vfs_init(void);
void vfs_mount(const char* path, vfs_node_t* local_root);
vfs_node_t* vfs_get_node(const char* path);

/* Standard VFS API wrappers */
uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void vfs_open(vfs_node_t *node);
void vfs_close(vfs_node_t *node);
struct dirent *vfs_readdir(vfs_node_t *node, uint32_t index);
vfs_node_t *vfs_finddir(vfs_node_t *node, char *name);
int vfs_ioctl(vfs_node_t *node, uint32_t request, void *arg);

#endif