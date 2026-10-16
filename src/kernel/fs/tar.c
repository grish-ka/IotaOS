/*
 * IotaOS - tar.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "fs/tar.h"
#include "fs/vfs.h"
#include "drivers/stdio.h"
#include "drivers/string.h"

static uint32_t octal_string_to_int(const char *str, int size) {
    uint32_t result = 0;
    for (int i = 0; i < size; i++) {
        if (str[i] >= '0' && str[i] <= '7') {
            result = result * 8 + (str[i] - '0');
        }
    }
    return result;
}

/* Join the ustar prefix and name fields, dropping a leading "./" and trailing "/" */
static void entry_path(const tar_header_t* header, char* out) {
    int o = 0;
    out[o++] = '/';
    for (int i = 0; i < 155 && header->prefix[i]; i++) out[o++] = header->prefix[i];
    if (o > 1) out[o++] = '/';
    const char* name = header->filename;
    if (name[0] == '.' && name[1] == '/') name += 2;
    for (int i = 0; i < 100 && name[i]; i++) out[o++] = name[i];
    while (o > 1 && out[o - 1] == '/') o--;
    out[o] = '\0';
}

int tar_load(uint32_t initrd_address) {
    uint32_t current_address = initrd_address;
    int count = 0;

    while (1) {
        tar_header_t* header = (tar_header_t*)current_address;
        if (header->filename[0] == '\0') break;

        uint32_t file_size = octal_string_to_int(header->size, 11);
        uint32_t mode = octal_string_to_int(header->mode, 8) & 07777;
        uint32_t mtime = octal_string_to_int(header->mtime, 12);
        uint32_t uid = octal_string_to_int(header->uid, 8);
        uint32_t gid = octal_string_to_int(header->gid, 8);
        char path[VFS_PATH_MAX];
        entry_path(header, path);

        if (header->typeflag == '5') {
            vfs_node_t* dir = vfs_mkdir_p(path);
            if (dir) {
                dir->mode = mode;
                dir->mtime = mtime;
                dir->uid = uid;
                dir->gid = gid;
            }
        } else if (header->typeflag == '0' || header->typeflag == '\0' || header->typeflag == '1') {
            char name[VFS_NAME_MAX];
            vfs_node_t* parent = vfs_lookup_parent("/", path, name);
            if (!parent) {
                /* Archives may omit directory entries; create them as needed */
                char dir_path[VFS_PATH_MAX];
                strcpy(dir_path, path);
                char* slash = dir_path + strlen(dir_path);
                while (*slash != '/') slash--;
                *slash = '\0';
                parent = vfs_mkdir_p(dir_path[0] ? dir_path : "/");
            }
            vfs_node_t* node = parent ? vfs_finddir(parent, name) : NULL;
            if (parent && !node) node = vfs_create(parent, name, FS_FILE, mode, NULL);
            if (node) {
                if (header->typeflag == '1') {
                    /* Hard link: share the target's (read-only) image data */
                    char target[VFS_PATH_MAX];
                    tar_header_t fake = *header;
                    for (int i = 0; i < 100; i++) fake.filename[i] = header->linkname[i];
                    for (int i = 0; i < 155; i++) fake.prefix[i] = 0;
                    entry_path(&fake, target);
                    vfs_node_t* t = vfs_lookup("/", target);
                    if (t) vfs_borrow(node, t->data, t->size);
                } else {
                    vfs_borrow(node, (uint8_t*)(current_address + 512), file_size);
                }
                node->mode = mode;
                node->mtime = mtime;
                node->uid = uid;
                node->gid = gid;
            }
        }

        count++;
        /* Links and directories have size 0, so they carry no data blocks */
        current_address += 512 + (((file_size + 511) / 512) * 512);
    }
    return count;
}
