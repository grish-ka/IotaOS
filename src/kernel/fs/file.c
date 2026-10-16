/*
 * IotaOS - file.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "fs/file.h"
#include "cpu/task.h"

#define MAX_FILES  256
#define MAX_PIPES  32
#define PIPE_SIZE  4096

#define EBADF   9
#define EAGAIN  11
#define EMFILE  24
#define EPIPE   32
#define ENFILE  23

typedef struct pipe {
    uint8_t buf[PIPE_SIZE];
    uint32_t read_pos;
    uint32_t count;
    int readers;
    int writers;
    int used;
} pipe_t;

static file_t files[MAX_FILES];
static pipe_t pipes[MAX_PIPES];

static file_t* alloc_file(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].refs == 0) {
            files[i].refs = 1;
            files[i].kind = 0;
            files[i].flags = 0;
            files[i].offset = 0;
            files[i].node = 0;
            files[i].pipe = 0;
            return &files[i];
        }
    }
    return 0;
}

file_t* file_open_node(vfs_node_t* node, int flags) {
    file_t* f = alloc_file();
    if (!f) return 0;
    f->kind = FILE_NODE;
    f->node = node;
    f->flags = flags;
    return f;
}

int file_create_pipe(file_t** read_end, file_t** write_end) {
    pipe_t* p = 0;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipes[i].used) { p = &pipes[i]; break; }
    }
    if (!p) return -ENFILE;

    file_t* r = alloc_file();
    if (!r) return -ENFILE;
    file_t* w = alloc_file();
    if (!w) { r->refs = 0; return -ENFILE; }

    p->used = 1;
    p->read_pos = 0;
    p->count = 0;
    p->readers = 1;
    p->writers = 1;
    r->kind = FILE_PIPE_READ;
    r->pipe = p;
    w->kind = FILE_PIPE_WRITE;
    w->flags = O_WRONLY;
    w->pipe = p;
    *read_end = r;
    *write_end = w;
    return 0;
}

void file_ref(file_t* f) {
    if (f) f->refs++;
}

void file_put(file_t* f) {
    if (!f || f->refs <= 0) return;
    if (--f->refs > 0) return;

    pipe_t* p = f->pipe;
    if (p) {
        if (f->kind == FILE_PIPE_READ) p->readers--;
        else p->writers--;
        if (p->readers <= 0 && p->writers <= 0) p->used = 0;
    }
}

int32_t file_read(file_t* f, uint8_t* buf, uint32_t count) {
    if (f->kind == FILE_PIPE_WRITE) return -EBADF;
    if (f->kind == FILE_NODE && (f->flags & O_ACCMODE) == O_WRONLY) return -EBADF;

    if (f->kind == FILE_PIPE_READ) {
        pipe_t* p = f->pipe;
        /* Block until there is data, or every writer has gone (end of file) */
        while (p->count == 0) {
            if (p->writers <= 0) return 0;
            if (f->flags & O_NONBLOCK) return -EAGAIN;
            task_yield();
        }
        uint32_t n = 0;
        while (n < count && p->count > 0) {
            buf[n++] = p->buf[p->read_pos];
            p->read_pos = (p->read_pos + 1) % PIPE_SIZE;
            p->count--;
        }
        return n;
    }

    int32_t n = vfs_read(f->node, f->offset, count, buf);
    if (n > 0 && VFS_TYPE(f->node) == FS_FILE) f->offset += n;
    return n;
}

int32_t file_write(file_t* f, const uint8_t* buf, uint32_t count) {
    if (f->kind == FILE_PIPE_READ) return -EBADF;
    if (f->kind == FILE_NODE && (f->flags & O_ACCMODE) == O_RDONLY) return -EBADF;

    if (f->kind == FILE_PIPE_WRITE) {
        pipe_t* p = f->pipe;
        uint32_t n = 0;
        while (n < count) {
            if (p->readers <= 0) return n ? (int32_t)n : -EPIPE;
            if (p->count == PIPE_SIZE) {
                if (f->flags & O_NONBLOCK) return n ? (int32_t)n : -EAGAIN;
                task_yield();
                continue;
            }
            uint32_t write_pos = (p->read_pos + p->count) % PIPE_SIZE;
            p->buf[write_pos] = buf[n++];
            p->count++;
        }
        return n;
    }

    if ((f->flags & O_APPEND) && VFS_TYPE(f->node) == FS_FILE) f->offset = f->node->size;
    int32_t n = vfs_write(f->node, f->offset, count, (uint8_t*)buf);
    if (n > 0 && VFS_TYPE(f->node) == FS_FILE) f->offset += n;
    return n;
}

int fd_install(fd_table_t* t, file_t* f, int min_fd) {
    for (int fd = min_fd; fd < MAX_FDS; fd++) {
        if (!t->fd[fd]) {
            t->fd[fd] = f;
            t->cloexec &= ~(1u << fd);
            return fd;
        }
    }
    return -EMFILE;
}

file_t* fd_get(fd_table_t* t, int fd) {
    if (fd < 0 || fd >= MAX_FDS) return 0;
    return t->fd[fd];
}

int fd_close(fd_table_t* t, int fd) {
    file_t* f = fd_get(t, fd);
    if (!f) return -EBADF;
    t->fd[fd] = 0;
    t->cloexec &= ~(1u << fd);
    file_put(f);
    return 0;
}

void fd_close_all(fd_table_t* t) {
    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (t->fd[fd]) fd_close(t, fd);
    }
}

void fd_close_on_exec(fd_table_t* t) {
    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (t->fd[fd] && (t->cloexec & (1u << fd))) fd_close(t, fd);
    }
}

void fd_clone(fd_table_t* dst, const fd_table_t* src) {
    for (int fd = 0; fd < MAX_FDS; fd++) {
        dst->fd[fd] = src->fd[fd];
        file_ref(dst->fd[fd]);
    }
    dst->cloexec = src->cloexec;
}
