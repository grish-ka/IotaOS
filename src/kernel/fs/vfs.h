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

#define VFS_TYPE(node) ((node)->flags & 0x07)

/* Errors are returned as negative values of these (they match libc errno) */
#define VFS_ENOENT   2
#define VFS_EBADF    9
#define VFS_ENOMEM   12
#define VFS_EEXIST   17
#define VFS_ENOTDIR  20
#define VFS_EISDIR   21
#define VFS_EINVAL   22
#define VFS_ENOSPC   28
#define VFS_ENAMETOOLONG 36
#define VFS_ENOTEMPTY 39

#define VFS_NAME_MAX 64
#define VFS_PATH_MAX 256

struct vfs_node;
struct file;

/* Device hooks; regular files and directories are handled by the VFS itself */
typedef uint32_t (*read_type_t)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
typedef int (*ioctl_type_t)(struct vfs_node*, uint32_t, void*);

/* Optional char-device hooks: poll() readiness, and last-close */
typedef uint32_t (*poll_type_t)(struct vfs_node*, uint32_t events);
typedef void (*close_type_t)(struct vfs_node*, struct file*);

/*
 * Every file, directory and device lives in one in-memory tree. File data
 * either borrows the ramdisk image (read-only, copied on first write) or
 * sits in pages the VFS owns.
 */
typedef struct vfs_node {
    char name[VFS_NAME_MAX];
    uint32_t mode;          /* permission bits, e.g. 0755 */
    uint32_t uid;
    uint32_t gid;
    uint32_t flags;         /* FS_FILE, FS_DIRECTORY, FS_CHARDEVICE */
    uint32_t inode;
    uint32_t size;
    uint32_t mtime;         /* seconds since the epoch */
    uint32_t nlink;

    uint8_t* data;          /* file contents */
    uint32_t capacity;      /* bytes owned at data; 0 when borrowing the ramdisk */

    struct vfs_node* parent;
    struct vfs_node* children;  /* first child (directories) */
    struct vfs_node* next;      /* next sibling */
    uint8_t used;

    read_type_t read;       /* devices only */
    write_type_t write;
    ioctl_type_t ioctl;
    poll_type_t poll;       /* optional: poll() readiness mask */
    close_type_t close;     /* optional: last reference to the node is going away */
} vfs_node_t;

/* Directory entry as returned to programs by SYS_GETDENTS */
struct dirent {
    uint32_t ino;
    uint8_t type;           /* FS_FILE / FS_DIRECTORY / FS_CHARDEVICE */
    char name[VFS_NAME_MAX];
};

extern vfs_node_t *vfs_root;

void vfs_init(void);

/* Canonicalise path against cwd into out (absolute, no ".", "..", or "//") */
int vfs_resolve_path(const char* cwd, const char* path, char* out);

/* Look up an absolute or cwd-relative path; NULL if missing */
vfs_node_t* vfs_lookup(const char* cwd, const char* path);

/* Find the parent directory of path and copy the final component to name */
vfs_node_t* vfs_lookup_parent(const char* cwd, const char* path, char* name);

vfs_node_t* vfs_finddir(vfs_node_t* dir, const char* name);
vfs_node_t* vfs_child_at(vfs_node_t* dir, uint32_t index);

/* Create name in dir; returns the node or NULL (errors via *err) */
vfs_node_t* vfs_create(vfs_node_t* dir, const char* name, uint32_t type, uint32_t mode, int* err);

/* Create every missing directory along an absolute path */
vfs_node_t* vfs_mkdir_p(const char* path);

int vfs_unlink(vfs_node_t* node);
int vfs_rename(vfs_node_t* node, vfs_node_t* new_parent, const char* new_name);
int vfs_truncate(vfs_node_t* node, uint32_t size);

/* Point a file at read-only data (the ramdisk) without copying it */
void vfs_borrow(vfs_node_t* node, uint8_t* data, uint32_t size);

/* Build the absolute path of node into out */
void vfs_get_path(vfs_node_t* node, char* out);

int32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
int32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
int vfs_ioctl(vfs_node_t *node, uint32_t request, void *arg);

/* Current wall-clock time, provided by the RTC driver */
uint32_t rtc_now(void);

#endif
