/*
 * IotaOS - syscall.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "cpu/isr.h"
#include "drivers/stdio.h"
#include "drivers/terminal.h"
#include "drivers/system.h"
#include "drivers/string.h"
#include "drivers/rtc.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "drivers/elf_loader.h"
#include "cpu/task.h"
#include "cpu/syscall.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "drivers/timer.h"

int current_exit_code = 0;

#define MAX_STRESS_PAGES 65536
uint32_t stress_pointers[MAX_STRESS_PAGES];
uint32_t stress_allocated = 0;

/* errno values returned (negated) to programs; they match libc's <errno.h> */
#define EPERM    1
#define ENOENT   2
#define ESRCH    3
#define E2BIG    7
#define ENOEXEC  8
#define EBADF    9
#define ECHILD   10
#define ENOMEM   12
#define EACCES   13
#define EEXIST   17
#define ENOTDIR  20
#define EISDIR   21
#define EINVAL   22
#define ENOTTY   25
#define ESPIPE   29
#define ENAMETOOLONG 36
#define ENOSYS   38

/* fcntl commands */
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_DUPFD_CLOEXEC 1030
#define FD_CLOEXEC 1

/* st_mode type bits */
#define S_IFIFO 0010000
#define S_IFCHR 0020000
#define S_IFDIR 0040000
#define S_IFREG 0100000

/*
 * Staging buffers for the program being started. They are copied into the new
 * image's own argument page (APP_ARGS_ADDR) within the same system call, so
 * programs starting at the same time cannot see each other's arguments.
 */
static char current_process_args[ARGS_SIZE];
static char current_process_env[ENV_SIZE];

/* Environment for programs init starts */
static const char default_env[] =
    "TERM=linux\0USER=root\0LOGNAME=root\0HOME=/root\0SHELL=/bin/bash\0PATH=/usr/local/bin:/usr/bin:/bin\0";

static const char* cwd(void) {
    return task_get_cwd();
}

static uint32_t node_mode(vfs_node_t* node) {
    switch (VFS_TYPE(node)) {
    case FS_DIRECTORY: return S_IFDIR | node->mode;
    case FS_CHARDEVICE: return S_IFCHR | node->mode;
    default: return S_IFREG | node->mode;
    }
}

static void fill_stat(vfs_node_t* node, kstat_t* st) {
    st->ino = node->inode;
    st->mode = node_mode(node);
    st->nlink = node->nlink;
    st->uid = node->uid;
    st->gid = node->gid;
    st->size = (VFS_TYPE(node) == FS_DIRECTORY) ? 4096 : node->size;
    st->mtime = node->mtime;
    st->rdev = (VFS_TYPE(node) == FS_CHARDEVICE) ? node->inode : 0;
}

static const char* basename_of(const char* path) {
    const char* base = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/' && p[1]) base = p + 1;
    }
    return base;
}

/* Map the program at path into a new address space */
static int load_program(const char* path, elf_image_t* image, vfs_node_t** found) {
    vfs_node_t* node = vfs_lookup(cwd(), path);
    if (found) *found = node;
    if (!node) return -ENOENT;
    if (VFS_TYPE(node) == FS_DIRECTORY) return -EISDIR;
    if (VFS_TYPE(node) != FS_FILE || !(node->mode & 0111)) return -EACCES;
    if (node->size < 4 || !node->data) return -ENOEXEC;

    /* File data is already in kernel-visible memory: load straight from it */
    if (elf_load(node->data, node->size, image) != 0) return -ENOEXEC;
    return 0;
}

/* Replace the current process with image; does not return */
static void exec_image(elf_image_t* image, const char* path, vfs_node_t* node) {
    /* Set-user-ID and set-group-ID programs run as the file's owner */
    if (node && (node->mode & 04000)) task_force_ids(node->uid, task_get_gid());
    if (node && (node->mode & 02000)) task_force_ids(task_get_uid(), node->gid);
    fd_close_on_exec(task_get_fds());
    task_set_name(basename_of(path));
    task_set_heap_end(TASK_HEAP_BASE);
    elf_run(image);
}

/*
 * Arguments reach the new program as a NUL-separated block ending in an empty
 * string. Legacy callers pass one space-separated string, split here.
 */
static int stage_args_string(const char* s) {
    uint32_t o = 0;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        while (*s && *s != ' ') {
            if (o >= ARGS_SIZE - 2) return -E2BIG;
            current_process_args[o++] = *s++;
        }
        current_process_args[o++] = '\0';
    }
    current_process_args[o] = '\0';
    if (o == 0) current_process_args[1] = '\0';
    return 0;
}

static uint32_t block_length(const char* block, uint32_t max) {
    uint32_t len = 0;
    while (len < max - 1 && !(block[len] == '\0' && block[len + 1] == '\0')) len++;
    return len;
}

/* Copy the staged argv and environment into image's argument page */
static void install_args(elf_image_t* image) {
    uint8_t* page = (uint8_t*)vmm_get_phys(image->page_directory, APP_ARGS_ADDR);
    uint32_t alen = block_length(current_process_args, ARGS_SIZE);
    uint32_t elen = block_length(current_process_env, ENV_SIZE);
    for (uint32_t k = 0; k < alen + 2; k++) page[APP_ARGS_OFFSET + k] = k < alen ? current_process_args[k] : 0;
    for (uint32_t k = 0; k < elen + 2; k++) page[APP_ENV_OFFSET + k] = k < elen ? current_process_env[k] : 0;
}

/* Start a program as a new child task (used by init and the legacy spawn call) */
int spawn_program(const char* path, const char* args) {
    elf_image_t image;
    if (load_program(path, &image, 0) != 0) return -1;

    stage_args_string(args);
    for (uint32_t k = 0; k < sizeof(default_env); k++) current_process_env[k] = default_env[k];
    install_args(&image);
    int pid = create_task((void (*)(void))image.entry, image.page_directory);
    if (pid < 0) {
        elf_unload(&image);
        return -1;
    }
    return pid;
}

/*
 * If path is a "#!" script, fill interp with its interpreter and new_args with
 * "interp [optional-arg] path argv[1]..." and return 1. Returns 0 for anything
 * else, or a negative errno if the script cannot be run.
 */
static int interpreter_for(const char* path, const char* args, char* interp, char* new_args) {
    vfs_node_t* node = vfs_lookup(cwd(), path);
    if (!node || VFS_TYPE(node) != FS_FILE || node->size < 3 || !node->data) return 0;
    if (node->data[0] != '#' || node->data[1] != '!') return 0;
    if (!(node->mode & 0111)) return -EACCES;

    /* Parse "#! /path/to/interp optional-arg" from the first line */
    const char* p = (const char*)node->data + 2;
    const char* end = (const char*)node->data + (node->size < 256 ? node->size : 256);
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    uint32_t n = 0;
    while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && n < VFS_PATH_MAX - 1) interp[n++] = *p++;
    interp[n] = '\0';
    if (n == 0) return -ENOEXEC;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    char opt[128];
    uint32_t on = 0;
    while (p < end && *p != '\n' && *p != '\r' && on < sizeof(opt) - 1) opt[on++] = *p++;
    while (on > 0 && (opt[on - 1] == ' ' || opt[on - 1] == '\t')) on--;
    opt[on] = '\0';

    /* New argv: interp [opt] path, then the original argv[1..] */
    uint32_t o = 0;
    const char* parts[3] = {interp, on ? opt : 0, path};
    for (int i = 0; i < 3; i++) {
        if (!parts[i]) continue;
        uint32_t len = strlen(parts[i]);
        if (o + len + 2 >= ARGS_SIZE) return -E2BIG;
        strcpy(new_args + o, parts[i]);
        o += len + 1;
    }
    const char* a = args + strlen(args) + 1; /* skip argv[0] */
    if (args[0]) {
        while (*a) {
            uint32_t len = strlen(a);
            if (o + len + 2 >= ARGS_SIZE) return -E2BIG;
            strcpy(new_args + o, a);
            o += len + 1;
            a += len + 1;
        }
    }
    new_args[o] = '\0';
    new_args[o + 1] = '\0';
    return 1;
}

static int sys_open(const char* path, int flags, uint32_t mode) {
    vfs_node_t* node = vfs_lookup(cwd(), path);

    if (!node) {
        if (!(flags & O_CREAT)) return -ENOENT;
        char name[VFS_NAME_MAX];
        vfs_node_t* parent = vfs_lookup_parent(cwd(), path, name);
        if (!parent) return -ENOENT;
        int err;
        node = vfs_create(parent, name, FS_FILE, mode, &err);
        if (!node) return err;
        node->uid = task_get_uid();
        node->gid = task_get_gid();
    } else if ((flags & O_CREAT) && (flags & O_EXCL)) {
        return -EEXIST;
    }

    if ((flags & O_DIRECTORY) && VFS_TYPE(node) != FS_DIRECTORY) return -ENOTDIR;
    if (VFS_TYPE(node) == FS_DIRECTORY && (flags & O_ACCMODE) != O_RDONLY) return -EISDIR;
    if ((flags & O_TRUNC) && (flags & O_ACCMODE) != O_RDONLY && VFS_TYPE(node) == FS_FILE) {
        int r = vfs_truncate(node, 0);
        if (r) return r;
    }

    file_t* f = file_open_node(node, flags & ~(O_CREAT | O_EXCL | O_TRUNC | O_CLOEXEC));
    if (!f) return -ENOMEM;
    int fd = fd_install(task_get_fds(), f, 0);
    if (fd < 0) {
        file_put(f);
        return fd;
    }
    if (flags & O_CLOEXEC) task_get_fds()->cloexec |= 1u << fd;
    return fd;
}

static int sys_getdents(int fd, struct dirent* out, uint32_t size) {
    file_t* f = fd_get(task_get_fds(), fd);
    if (!f) return -EBADF;
    if (f->kind != FILE_NODE || VFS_TYPE(f->node) != FS_DIRECTORY) return -ENOTDIR;

    uint32_t max = size / sizeof(struct dirent);
    uint32_t n = 0;
    while (n < max) {
        vfs_node_t* entry;
        const char* name;
        if (f->offset == 0) { entry = f->node; name = "."; }
        else if (f->offset == 1) { entry = f->node->parent; name = ".."; }
        else {
            entry = vfs_child_at(f->node, f->offset - 2);
            if (!entry) break;
            name = entry->name;
        }
        out[n].ino = entry->inode;
        out[n].type = VFS_TYPE(entry);
        strcpy(out[n].name, name);
        n++;
        f->offset++;
    }
    return n * sizeof(struct dirent);
}

static int sys_fcntl(int fd, int cmd, int arg) {
    fd_table_t* t = task_get_fds();
    file_t* f = fd_get(t, fd);
    if (!f) return -EBADF;

    switch (cmd) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC: {
        file_ref(f);
        int nfd = fd_install(t, f, arg);
        if (nfd < 0) { file_put(f); return nfd; }
        if (cmd == F_DUPFD_CLOEXEC) t->cloexec |= 1u << nfd;
        return nfd;
    }
    case F_GETFD:
        return (t->cloexec & (1u << fd)) ? FD_CLOEXEC : 0;
    case F_SETFD:
        if (arg & FD_CLOEXEC) t->cloexec |= 1u << fd;
        else t->cloexec &= ~(1u << fd);
        return 0;
    case F_GETFL:
        return f->flags;
    case F_SETFL:
        f->flags = (f->flags & ~(O_APPEND | O_NONBLOCK)) | (arg & (O_APPEND | O_NONBLOCK));
        return 0;
    }
    return -EINVAL;
}

/* Copy the exec arguments (a NUL-separated block) and environment out of the
   caller before its memory goes */
static int stage_exec(const char* args, const char* env) {
    uint32_t args_len = block_length(args, ARGS_SIZE);
    if (args_len >= ARGS_SIZE - 2) return -E2BIG;
    uint32_t env_len = 0;
    if (env) {
        env_len = block_length(env, ENV_SIZE);
        if (env_len >= ENV_SIZE - 2) return -E2BIG;
    }

    for (uint32_t k = 0; k < args_len; k++) current_process_args[k] = args[k];
    current_process_args[args_len] = '\0';
    current_process_args[args_len + 1] = '\0';
    if (env) {
        for (uint32_t k = 0; k < env_len; k++) current_process_env[k] = env[k];
        current_process_env[env_len] = '\0';
        current_process_env[env_len + 1] = '\0';
    }
    return 0;
}

void syscall_handler(registers_t *regs) {
    fd_table_t* fds = task_get_fds();

    switch (regs->eax) {
        case SYS_PANIC:
            panic((char*)regs->ebx);
            break;

        case SYS_REBOOT:
            reboot();
            break;

        case SYS_READ:
        {
            file_t* f = fd_get(fds, (int)regs->ebx);
            regs->eax = f ? file_read(f, (uint8_t*)regs->ecx, regs->edx) : -EBADF;
            break;
        }

        case SYS_WRITE:
        {
            file_t* f = fd_get(fds, (int)regs->ebx);
            regs->eax = f ? file_write(f, (const uint8_t*)regs->ecx, regs->edx) : -EBADF;
            break;
        }

        case SYS_MEMINFO:
            regs->eax = pmm_get_free_block_count();
            regs->ebx = pmm_get_total_block_count();
            break;

        case SYS_EXIT:
            current_exit_code = regs->ebx;
            task_exit((regs->ebx & 0xff) << 8);
            break;

        case SYS_EXEC:  /* legacy: ebx = "path arg arg..." */
        case SYS_SPAWN:
        {
            char* full_cmd = (char*)regs->ebx;
            char filename[VFS_PATH_MAX] = {0};
            int i = 0;
            while (full_cmd[i] != ' ' && full_cmd[i] != '\0' && i < VFS_PATH_MAX - 1) {
                filename[i] = full_cmd[i];
                i++;
            }

            if (regs->eax == SYS_SPAWN) {
                int pid = spawn_program(filename, full_cmd);
                regs->eax = pid < 0 ? (uint32_t)-1 : (uint32_t)pid;
                break;
            }

            elf_image_t image;
            vfs_node_t* node;
            int r = load_program(filename, &image, &node);
            if (r != 0) {
                printf("Error: File '%s' not found.\n", filename);
                regs->eax = r;
                break;
            }
            if (stage_args_string(full_cmd) != 0) {
                elf_unload(&image);
                regs->eax = -E2BIG;
                break;
            }
            /* Same environment as the caller */
            const char* old_env = (const char*)(APP_ARGS_ADDR + APP_ENV_OFFSET);
            uint32_t elen = block_length(old_env, ENV_SIZE);
            for (uint32_t k = 0; k < elen + 2; k++) current_process_env[k] = k < elen ? old_env[k] : 0;
            install_args(&image);
            exec_image(&image, filename, node);
            break;
        }

        case SYS_LS: /* legacy listing used by iosh */
        {
            const char* target = (const char*)regs->ebx;
            vfs_node_t* dir = vfs_lookup(cwd(), target[0] ? target : ".");
            if (!dir || VFS_TYPE(dir) != FS_DIRECTORY) {
                printf("ls: cannot access '%s': No such directory\n", target);
                break;
            }
            for (vfs_node_t* c = dir->children; c; c = c->next) printf("%s ", c->name);
            printf("\n");
            break;
        }

        case SYS_SLEEP: /* ebx = milliseconds */
        {
            uint32_t ticks = regs->ebx / 10;
            __asm__ volatile("sti");
            timer_sleep(ticks ? ticks : 1);
            __asm__ volatile("cli");
            break;
        }

        case SYS_YIELD:
            task_yield();
            break;

        case SYS_PUTCHAR_AT:
        {
            char c = (char)regs->ebx;
            uint8_t color = (uint8_t)regs->ecx;
            size_t x = (size_t)(regs->edx >> 16);
            size_t y = (size_t)(regs->edx & 0xFFFF);
            terminal_putentryat(c, color, x, y);
            break;
        }

        case SYS_STRESS_RAM:
        {
            if (regs->ebx == 1) {
                uint32_t pages_to_alloc = regs->ecx * 256;
                uint32_t allocated_this_run = 0;
                while (allocated_this_run < pages_to_alloc && stress_allocated < MAX_STRESS_PAGES) {
                    void* block = pmm_alloc_block();
                    if (!block) break;
                    stress_pointers[stress_allocated++] = (uint32_t)block;
                    allocated_this_run++;
                }
            }
            else if (regs->ebx == 0) {
                for (uint32_t i = 0; i < stress_allocated; i++) {
                    pmm_free_block((void*)stress_pointers[i]);
                }
                stress_allocated = 0;
            }
            break;
        }

        case SYS_CLEAR:
            terminal_clear();
            break;

        case SYS_BRK:
        {
            int increment = (int)regs->ebx;
            if (increment == 0) {
                regs->eax = task_get_heap_end();
            } else {
                uint32_t old_break = task_get_heap_end();
                uint32_t new_break = old_break + increment;

                uint32_t start_page = (old_break + 0xFFF) & ~0xFFF;
                uint32_t end_page = (new_break + 0xFFF) & ~0xFFF;

                uint32_t cr3;
                __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
                uint32_t* current_dir = (uint32_t*)cr3;

                for (uint32_t virt = start_page; virt < end_page; virt += 4096) {
                    uint32_t phys = (uint32_t)pmm_alloc_block();
                    if (phys) {
                        vmm_map_page(current_dir, virt, phys, 7);
                    } else {
                        regs->eax = -1;
                        return;
                    }
                }

                task_set_heap_end(new_break);
                regs->eax = old_break;
            }
            break;
        }

        case SYS_OPEN: /* ebx = path, ecx = flags, edx = mode */
            regs->eax = sys_open((const char*)regs->ebx, (int)regs->ecx, regs->edx);
            break;

        case SYS_CLOSE:
            regs->eax = fd_close(fds, (int)regs->ebx);
            break;

        case SYS_GET_ARGS: /* ebx = buffer of ARGS_SIZE; gets the NUL-separated block */
        {
            char* buf = (char*)regs->ebx;
            const char* args = (const char*)(APP_ARGS_ADDR + APP_ARGS_OFFSET);
            uint32_t len = block_length(args, ARGS_SIZE);
            for (uint32_t k = 0; k < len; k++) buf[k] = args[k];
            buf[len] = '\0';
            buf[len + 1] = '\0';
            regs->eax = len;
            break;
        }

        case SYS_FORK:
            regs->eax = task_fork(regs);
            break;

        case SYS_WAIT:
            task_wait((int)regs->ebx);
            regs->eax = 0;
            break;

        case SYS_GET_ENV:
        {
            char* buf = (char*)regs->ebx;
            const char* env = (const char*)(APP_ARGS_ADDR + APP_ENV_OFFSET);
            int count = 0;
            while (!(env[count] == '\0' && env[count+1] == '\0') && count < ENV_SIZE - 2) {
                buf[count] = env[count];
                count++;
            }
            buf[count] = '\0';
            buf[count+1] = '\0';
            regs->eax = count + 1;
            break;
        }

        case SYS_CHDIR:
        {
            char abs_path[VFS_PATH_MAX];
            if (vfs_resolve_path(cwd(), (const char*)regs->ebx, abs_path) != 0) {
                regs->eax = -ENAMETOOLONG;
                break;
            }
            vfs_node_t* node = vfs_lookup("/", abs_path);
            if (!node) regs->eax = -ENOENT;
            else if (VFS_TYPE(node) != FS_DIRECTORY) regs->eax = -ENOTDIR;
            else {
                task_set_cwd(abs_path);
                regs->eax = 0;
            }
            break;
        }

        case SYS_GETCWD:
            strcpy((char*)regs->ebx, cwd());
            regs->eax = 0;
            break;

        case SYS_IOCTL:
        {
            file_t* f = fd_get(fds, (int)regs->ebx);
            if (!f) regs->eax = -EBADF;
            else if (f->kind != FILE_NODE) regs->eax = -ENOTTY;
            else regs->eax = vfs_ioctl(f->node, regs->ecx, (void*)regs->edx);
            break;
        }

        case SYS_EXECVE: /* ebx=path, ecx=NUL-separated argv block, edx=NUL-separated env or 0 */
        {
            const char* path = (const char*)regs->ebx;
            const char* args = (const char*)regs->ecx;
            static char interp[VFS_PATH_MAX];
            static char script_args[ARGS_SIZE];
            int r = interpreter_for(path, args, interp, script_args);
            if (r < 0) {
                regs->eax = r;
                break;
            }
            if (r > 0) {
                /* "#!" script: run its interpreter with the script as an argument */
                path = interp;
                args = script_args;
            }

            elf_image_t image;
            vfs_node_t* node;
            r = load_program(path, &image, &node);
            if (r != 0) {
                regs->eax = r;
                break;
            }
            /* A null environment means "keep the current one" */
            const char* env = (const char*)regs->edx;
            if (!env) env = (const char*)(APP_ARGS_ADDR + APP_ENV_OFFSET);
            r = stage_exec(args, env);
            if (r != 0) {
                elf_unload(&image);
                regs->eax = r;
                break;
            }
            install_args(&image);
            exec_image(&image, path, node);
            break;
        }

        case SYS_WAITPID: /* ebx=pid (-1 any), ecx=int* status, edx=nonzero for WNOHANG */
        {
            int status = 0;
            int pid = task_waitpid((int)regs->ebx, &status, (int)regs->edx);
            if (pid > 0 && regs->ecx) *(int*)regs->ecx = status;
            regs->eax = pid < 0 ? -ECHILD : pid;
            break;
        }

        case SYS_GETPID:
            regs->eax = current_task;
            break;

        case SYS_GETPPID:
            regs->eax = task_get_ppid();
            break;

        case SYS_STAT: /* ebx=path, ecx=kstat_t* */
        {
            vfs_node_t* node = vfs_lookup(cwd(), (const char*)regs->ebx);
            if (!node) { regs->eax = -ENOENT; break; }
            fill_stat(node, (kstat_t*)regs->ecx);
            regs->eax = 0;
            break;
        }

        case SYS_LSEEK: /* ebx=fd, ecx=offset, edx=whence; returns the new offset */
        {
            file_t* f = fd_get(fds, (int)regs->ebx);
            if (!f) { regs->eax = -EBADF; break; }
            if (f->kind != FILE_NODE || VFS_TYPE(f->node) == FS_CHARDEVICE) { regs->eax = -ESPIPE; break; }
            int32_t base = 0;
            if (regs->edx == 1) base = f->offset;
            else if (regs->edx == 2) base = f->node->size;
            else if (regs->edx != 0) { regs->eax = -EINVAL; break; }
            int32_t pos = base + (int32_t)regs->ecx;
            if (pos < 0) { regs->eax = -EINVAL; break; }
            f->offset = pos;
            regs->eax = pos;
            break;
        }

        case SYS_DUP:
        {
            file_t* f = fd_get(fds, (int)regs->ebx);
            if (!f) { regs->eax = -EBADF; break; }
            file_ref(f);
            int nfd = fd_install(fds, f, 0);
            if (nfd < 0) file_put(f);
            regs->eax = nfd;
            break;
        }

        case SYS_DUP2:
        {
            int oldfd = (int)regs->ebx;
            int newfd = (int)regs->ecx;
            file_t* f = fd_get(fds, oldfd);
            if (!f || newfd < 0 || newfd >= MAX_FDS) { regs->eax = -EBADF; break; }
            if (oldfd != newfd) {
                file_ref(f);
                if (fds->fd[newfd]) fd_close(fds, newfd);
                fd_install(fds, f, newfd);
            }
            regs->eax = newfd;
            break;
        }

        case SYS_PIPE: /* ebx = int[2] */
        {
            file_t *r, *w;
            int err = file_create_pipe(&r, &w);
            if (err) { regs->eax = err; break; }
            int rfd = fd_install(fds, r, 0);
            int wfd = rfd < 0 ? rfd : fd_install(fds, w, 0);
            if (rfd < 0 || wfd < 0) {
                if (rfd >= 0) fd_close(fds, rfd);
                else file_put(r);
                file_put(w);
                regs->eax = rfd < 0 ? rfd : wfd;
                break;
            }
            ((int*)regs->ebx)[0] = rfd;
            ((int*)regs->ebx)[1] = wfd;
            regs->eax = 0;
            break;
        }

        case SYS_FSTAT: /* ebx=fd, ecx=kstat_t* */
        {
            file_t* f = fd_get(fds, (int)regs->ebx);
            if (!f) { regs->eax = -EBADF; break; }
            kstat_t* st = (kstat_t*)regs->ecx;
            if (f->kind == FILE_NODE) {
                fill_stat(f->node, st);
            } else {
                st->ino = 0;
                st->mode = S_IFIFO | 0600;
                st->nlink = 1;
                st->uid = st->gid = 0;
                st->size = 0;
                st->mtime = rtc_now();
                st->rdev = 0;
            }
            regs->eax = 0;
            break;
        }

        case SYS_MKDIR: /* ebx=path, ecx=mode */
        {
            char name[VFS_NAME_MAX];
            const char* path = (const char*)regs->ebx;
            if (vfs_lookup(cwd(), path)) { regs->eax = -EEXIST; break; }
            vfs_node_t* parent = vfs_lookup_parent(cwd(), path, name);
            if (!parent) { regs->eax = -ENOENT; break; }
            int err;
            vfs_node_t* dir = vfs_create(parent, name, FS_DIRECTORY, regs->ecx, &err);
            if (dir) {
                dir->uid = task_get_uid();
                dir->gid = task_get_gid();
            }
            regs->eax = err;
            break;
        }

        case SYS_UNLINK:
        case SYS_RMDIR:
        {
            vfs_node_t* node = vfs_lookup(cwd(), (const char*)regs->ebx);
            if (!node) { regs->eax = -ENOENT; break; }
            int is_dir = VFS_TYPE(node) == FS_DIRECTORY;
            if (regs->eax == SYS_UNLINK && is_dir) { regs->eax = -EISDIR; break; }
            if (regs->eax == SYS_RMDIR && !is_dir) { regs->eax = -ENOTDIR; break; }
            if (VFS_TYPE(node) == FS_CHARDEVICE) { regs->eax = -EPERM; break; }
            regs->eax = vfs_unlink(node);
            break;
        }

        case SYS_RENAME: /* ebx=old, ecx=new */
        {
            vfs_node_t* node = vfs_lookup(cwd(), (const char*)regs->ebx);
            if (!node) { regs->eax = -ENOENT; break; }
            char name[VFS_NAME_MAX];
            vfs_node_t* parent = vfs_lookup_parent(cwd(), (const char*)regs->ecx, name);
            if (!parent) { regs->eax = -ENOENT; break; }
            regs->eax = vfs_rename(node, parent, name);
            break;
        }

        case SYS_GETDENTS: /* ebx=fd, ecx=struct dirent*, edx=bytes; returns bytes filled */
            regs->eax = sys_getdents((int)regs->ebx, (struct dirent*)regs->ecx, regs->edx);
            break;

        case SYS_FCNTL:
            regs->eax = sys_fcntl((int)regs->ebx, (int)regs->ecx, (int)regs->edx);
            break;

        case SYS_TIME: /* returns seconds since the epoch; ecx (if set) gets ticks */
            if (regs->ecx) *(uint32_t*)regs->ecx = timer_get_ticks();
            regs->eax = rtc_now();
            break;

        case SYS_PROCLIST: /* ebx=task_info_t*, ecx=max; returns count */
            regs->eax = task_list((task_info_t*)regs->ebx, (int)regs->ecx);
            break;

        case SYS_KILL:
            regs->eax = task_kill((int)regs->ebx, (int)regs->ecx);
            break;

        case SYS_FTRUNCATE:
        {
            file_t* f = fd_get(fds, (int)regs->ebx);
            if (!f) regs->eax = -EBADF;
            else if (f->kind != FILE_NODE) regs->eax = -EINVAL;
            else regs->eax = vfs_truncate(f->node, regs->ecx);
            break;
        }

        case SYS_SYSINFO: /* ebx=ksysinfo_t* */
        {
            ksysinfo_t* info = (ksysinfo_t*)regs->ebx;
            info->uptime_ticks = timer_get_ticks();
            info->ticks_per_second = 100;
            info->total_pages = pmm_get_total_block_count();
            info->free_pages = pmm_get_free_block_count();
            task_info_t list[MAX_TASKS];
            info->procs = task_list(list, MAX_TASKS);
            regs->eax = 0;
            break;
        }

        case SYS_CHMOD:
        {
            vfs_node_t* node = vfs_lookup(cwd(), (const char*)regs->ebx);
            if (!node) { regs->eax = -ENOENT; break; }
            node->mode = regs->ecx & 07777;
            regs->eax = 0;
            break;
        }

        case SYS_CHOWN: /* ebx=path, ecx=uid, edx=gid (-1 = unchanged) */
        {
            vfs_node_t* node = vfs_lookup(cwd(), (const char*)regs->ebx);
            if (!node) { regs->eax = -ENOENT; break; }
            if (task_get_uid() != 0) { regs->eax = -EPERM; break; }
            if (regs->ecx != 0xFFFFFFFF) node->uid = regs->ecx;
            if (regs->edx != 0xFFFFFFFF) node->gid = regs->edx;
            regs->eax = 0;
            break;
        }

        case SYS_UTIME: /* ebx=path, ecx=mtime (0 = now) */
        {
            vfs_node_t* node = vfs_lookup(cwd(), (const char*)regs->ebx);
            if (!node) { regs->eax = -ENOENT; break; }
            node->mtime = regs->ecx ? regs->ecx : rtc_now();
            regs->eax = 0;
            break;
        }

        case SYS_GETIDS: /* returns uid, and gid in EBX */
            regs->eax = task_get_uid();
            regs->ebx = task_get_gid();
            break;

        case SYS_SETIDS: /* ebx=uid, ecx=gid */
            regs->eax = task_set_ids(regs->ebx, regs->ecx) == 0 ? 0 : -EPERM;
            break;

        case SYS_POWEROFF:
            /* QEMU (new and old) and Bochs ACPI power-off ports, then halt */
            __asm__ volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
            __asm__ volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0xB004));
            shutdown();
            break;

        default:
            regs->eax = -ENOSYS;
            break;
    }
}
