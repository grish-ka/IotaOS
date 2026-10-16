/*
 * IotaOS - file.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include "fs/vfs.h"

#define MAX_FDS 32

/* open() flags, matching libc's <fcntl.h> */
#define O_ACCMODE   0x0003
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_NOCTTY    0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800
#define O_DIRECTORY 0x10000
#define O_CLOEXEC   0x80000

#define FILE_NODE       1
#define FILE_PIPE_READ  2
#define FILE_PIPE_WRITE 3

struct pipe;

/* An open file description: shared by dup()ed and fork()ed descriptors */
typedef struct file {
    int refs;
    int kind;
    int flags;
    uint32_t offset;
    vfs_node_t* node;
    struct pipe* pipe;
} file_t;

/* A process's descriptor table */
typedef struct fd_table {
    file_t* fd[MAX_FDS];
    uint32_t cloexec;   /* bit n set: close fd n on exec */
} fd_table_t;

file_t* file_open_node(vfs_node_t* node, int flags);
int file_create_pipe(file_t** read_end, file_t** write_end);
void file_ref(file_t* f);
void file_put(file_t* f);

int32_t file_read(file_t* f, uint8_t* buf, uint32_t count);
int32_t file_write(file_t* f, const uint8_t* buf, uint32_t count);

/* Lowest free descriptor >= min_fd for f (which it takes over), or -EMFILE */
int fd_install(fd_table_t* t, file_t* f, int min_fd);
file_t* fd_get(fd_table_t* t, int fd);
int fd_close(fd_table_t* t, int fd);
void fd_close_all(fd_table_t* t);
void fd_close_on_exec(fd_table_t* t);
/* Copy a table for fork(): every open file gains a reference */
void fd_clone(fd_table_t* dst, const fd_table_t* src);

#endif
