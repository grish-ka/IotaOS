/*
 * IotaOS - vfs.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "fs/vfs.h"
#include "mem/pmm.h"
#include "drivers/string.h"

#define MAX_NODES 1024

static vfs_node_t nodes[MAX_NODES];
static uint32_t next_inode = 1;
vfs_node_t *vfs_root = NULL;

static void vfs_memcpy(uint8_t* dst, const uint8_t* src, uint32_t n) {
    while (n--) *dst++ = *src++;
}

static void vfs_memset(uint8_t* dst, uint8_t v, uint32_t n) {
    while (n--) *dst++ = v;
}

static vfs_node_t* alloc_node(void) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (!nodes[i].used) {
            vfs_memset((uint8_t*)&nodes[i], 0, sizeof(vfs_node_t));
            nodes[i].used = 1;
            nodes[i].inode = next_inode++;
            nodes[i].nlink = 1;
            return &nodes[i];
        }
    }
    return NULL;
}

void vfs_init(void) {
    vfs_root = alloc_node();
    vfs_root->name[0] = '/';
    vfs_root->flags = FS_DIRECTORY;
    vfs_root->mode = 0755;
    vfs_root->parent = vfs_root;
    vfs_root->nlink = 2;
}

int vfs_resolve_path(const char* cwd, const char* path, char* out) {
    char tmp[VFS_PATH_MAX * 2];
    uint32_t len = 0;

    if (path[0] != '/') {
        uint32_t cl = strlen(cwd);
        if (cl >= sizeof(tmp) - 2) return -VFS_ENAMETOOLONG;
        strcpy(tmp, cwd);
        len = cl;
        tmp[len++] = '/';
    }
    uint32_t pl = strlen(path);
    if (len + pl >= sizeof(tmp)) return -VFS_ENAMETOOLONG;
    strcpy(tmp + len, path);

    /* Walk the components, keeping a stack of where each one starts in out */
    uint32_t o = 0;
    uint32_t starts[VFS_PATH_MAX / 2];
    int depth = 0;
    const char* p = tmp;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char* seg = p;
        while (*p && *p != '/') p++;
        uint32_t sl = p - seg;

        if (sl == 1 && seg[0] == '.') continue;
        if (sl == 2 && seg[0] == '.' && seg[1] == '.') {
            if (depth > 0) o = starts[--depth];
            continue;
        }
        if (sl >= VFS_NAME_MAX) return -VFS_ENAMETOOLONG;
        if (o + sl + 2 >= VFS_PATH_MAX || depth >= (int)(sizeof(starts) / sizeof(starts[0]))) {
            return -VFS_ENAMETOOLONG;
        }
        starts[depth++] = o;
        out[o++] = '/';
        for (uint32_t i = 0; i < sl; i++) out[o++] = seg[i];
    }
    if (o == 0) out[o++] = '/';
    out[o] = '\0';
    return 0;
}

vfs_node_t* vfs_finddir(vfs_node_t* dir, const char* name) {
    if (!dir || VFS_TYPE(dir) != FS_DIRECTORY) return NULL;
    for (vfs_node_t* c = dir->children; c; c = c->next) {
        if (strcmp(c->name, name) == 0) return c;
    }
    return NULL;
}

vfs_node_t* vfs_child_at(vfs_node_t* dir, uint32_t index) {
    vfs_node_t* c = dir->children;
    while (c && index--) c = c->next;
    return c;
}

vfs_node_t* vfs_lookup(const char* cwd, const char* path) {
    char abs[VFS_PATH_MAX];
    if (vfs_resolve_path(cwd, path, abs) != 0) return NULL;

    vfs_node_t* cur = vfs_root;
    const char* p = abs;
    char name[VFS_NAME_MAX];
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        int i = 0;
        while (*p && *p != '/') name[i++] = *p++;
        name[i] = '\0';
        cur = vfs_finddir(cur, name);
        if (!cur) return NULL;
    }
    return cur;
}

vfs_node_t* vfs_lookup_parent(const char* cwd, const char* path, char* name) {
    char abs[VFS_PATH_MAX];
    if (vfs_resolve_path(cwd, path, abs) != 0) return NULL;
    if (abs[1] == '\0') return NULL; /* "/" has no parent entry */

    char* slash = abs + strlen(abs);
    while (*slash != '/') slash--;
    strcpy(name, slash + 1);
    if (slash == abs) return vfs_root;
    *slash = '\0';
    vfs_node_t* parent = vfs_lookup("/", abs);
    if (parent && VFS_TYPE(parent) != FS_DIRECTORY) return NULL;
    return parent;
}

vfs_node_t* vfs_create(vfs_node_t* dir, const char* name, uint32_t type, uint32_t mode, int* err) {
    int dummy;
    if (!err) err = &dummy;
    if (!dir || VFS_TYPE(dir) != FS_DIRECTORY) { *err = -VFS_ENOTDIR; return NULL; }
    if (!name[0] || strlen(name) >= VFS_NAME_MAX) { *err = -VFS_ENAMETOOLONG; return NULL; }
    if (vfs_finddir(dir, name)) { *err = -VFS_EEXIST; return NULL; }

    vfs_node_t* n = alloc_node();
    if (!n) { *err = -VFS_ENOSPC; return NULL; }
    strcpy(n->name, name);
    n->flags = type;
    n->mode = mode & 07777;
    n->mtime = rtc_now();
    n->parent = dir;
    if (type == FS_DIRECTORY) {
        n->nlink = 2;
        dir->nlink++;
    }

    /* Keep children sorted by name, as ls would show them */
    vfs_node_t** link = &dir->children;
    while (*link && strcmp((*link)->name, name) < 0) link = &(*link)->next;
    n->next = *link;
    *link = n;
    dir->mtime = n->mtime;
    *err = 0;
    return n;
}

vfs_node_t* vfs_mkdir_p(const char* path) {
    char abs[VFS_PATH_MAX];
    if (vfs_resolve_path("/", path, abs) != 0) return NULL;

    vfs_node_t* cur = vfs_root;
    const char* p = abs;
    char name[VFS_NAME_MAX];
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        int i = 0;
        while (*p && *p != '/') name[i++] = *p++;
        name[i] = '\0';
        vfs_node_t* next = vfs_finddir(cur, name);
        if (!next) next = vfs_create(cur, name, FS_DIRECTORY, 0755, NULL);
        if (!next || VFS_TYPE(next) != FS_DIRECTORY) return NULL;
        cur = next;
    }
    return cur;
}

static void free_data(vfs_node_t* node) {
    if (node->capacity) {
        for (uint32_t off = 0; off < node->capacity; off += PAGE_SIZE) {
            pmm_free_block(node->data + off);
        }
    }
    node->data = NULL;
    node->capacity = 0;
}

static void detach(vfs_node_t* node) {
    vfs_node_t** link = &node->parent->children;
    while (*link && *link != node) link = &(*link)->next;
    if (*link) *link = node->next;
    node->next = NULL;
}

int vfs_unlink(vfs_node_t* node) {
    if (!node || node == vfs_root) return -VFS_EINVAL;
    if (VFS_TYPE(node) == FS_DIRECTORY) {
        if (node->children) return -VFS_ENOTEMPTY;
        node->parent->nlink--;
    }
    detach(node);
    node->parent->mtime = rtc_now();
    free_data(node);
    node->used = 0;
    return 0;
}

int vfs_rename(vfs_node_t* node, vfs_node_t* new_parent, const char* new_name) {
    if (!node || node == vfs_root || !new_parent) return -VFS_EINVAL;
    if (strlen(new_name) >= VFS_NAME_MAX) return -VFS_ENAMETOOLONG;

    /* A directory cannot move inside itself */
    for (vfs_node_t* p = new_parent; p != vfs_root; p = p->parent) {
        if (p == node) return -VFS_EINVAL;
    }

    vfs_node_t* existing = vfs_finddir(new_parent, new_name);
    if (existing == node) return 0;
    if (existing) {
        if (VFS_TYPE(existing) == FS_DIRECTORY && VFS_TYPE(node) != FS_DIRECTORY) return -VFS_EISDIR;
        if (VFS_TYPE(existing) != FS_DIRECTORY && VFS_TYPE(node) == FS_DIRECTORY) return -VFS_ENOTDIR;
        int r = vfs_unlink(existing);
        if (r != 0) return r;
    }

    detach(node);
    if (VFS_TYPE(node) == FS_DIRECTORY) {
        node->parent->nlink--;
        new_parent->nlink++;
    }
    strcpy(node->name, new_name);
    node->parent = new_parent;
    vfs_node_t** link = &new_parent->children;
    while (*link && strcmp((*link)->name, new_name) < 0) link = &(*link)->next;
    node->next = *link;
    *link = node;
    new_parent->mtime = rtc_now();
    return 0;
}

/* Make sure the file owns at least min_capacity bytes of writable storage */
static int ensure_capacity(vfs_node_t* node, uint32_t min_capacity) {
    if (node->capacity >= min_capacity && node->capacity) return 0;

    uint32_t new_cap = node->capacity ? node->capacity : PAGE_SIZE;
    while (new_cap < min_capacity) new_cap *= 2;
    uint32_t pages = new_cap / PAGE_SIZE;

    uint8_t* new_data = pmm_alloc_blocks(pages);
    if (!new_data) return -VFS_ENOSPC;
    vfs_memset(new_data, 0, new_cap);
    if (node->data && node->size) {
        vfs_memcpy(new_data, node->data, node->size < new_cap ? node->size : new_cap);
    }

    free_data(node);
    node->data = new_data;
    node->capacity = new_cap;
    return 0;
}

int vfs_truncate(vfs_node_t* node, uint32_t size) {
    if (VFS_TYPE(node) == FS_DIRECTORY) return -VFS_EISDIR;
    if (VFS_TYPE(node) != FS_FILE) return 0;
    if (size == 0) {
        free_data(node);
    } else {
        int r = ensure_capacity(node, size);
        if (r) return r;
        if (size > node->size) vfs_memset(node->data + node->size, 0, size - node->size);
    }
    node->size = size;
    node->mtime = rtc_now();
    return 0;
}

void vfs_borrow(vfs_node_t* node, uint8_t* data, uint32_t size) {
    free_data(node);
    node->data = data;
    node->size = size;
}

void vfs_get_path(vfs_node_t* node, char* out) {
    if (node == vfs_root) {
        strcpy(out, "/");
        return;
    }
    const char* parts[64];
    int n = 0;
    for (vfs_node_t* p = node; p != vfs_root && n < 64; p = p->parent) parts[n++] = p->name;
    out[0] = '\0';
    while (n--) {
        strcat(out, "/");
        strcat(out, parts[n]);
    }
}

int32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node) return -VFS_EBADF;
    if (node->read) return node->read(node, offset, size, buffer);
    if (VFS_TYPE(node) == FS_DIRECTORY) return -VFS_EISDIR;
    if (offset >= node->size) return 0;
    if (size > node->size - offset) size = node->size - offset;
    vfs_memcpy(buffer, node->data + offset, size);
    return size;
}

int32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node) return -VFS_EBADF;
    if (node->write) return node->write(node, offset, size, buffer);
    if (VFS_TYPE(node) == FS_DIRECTORY) return -VFS_EISDIR;
    if (size == 0) return 0;
    int r = ensure_capacity(node, offset + size);
    if (r) return r;
    if (offset > node->size) vfs_memset(node->data + node->size, 0, offset - node->size);
    vfs_memcpy(node->data + offset, buffer, size);
    if (offset + size > node->size) node->size = offset + size;
    node->mtime = rtc_now();
    return size;
}

int vfs_ioctl(vfs_node_t *node, uint32_t request, void *arg) {
    if (node && node->ioctl) return node->ioctl(node, request, arg);
    return -25; /* ENOTTY */
}
