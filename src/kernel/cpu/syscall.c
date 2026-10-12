/*
 * IotaOS - syscall.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "cpu/isr.h"
#include "drivers/stdio.h"
#include "drivers/terminal.h"
#include "drivers/system.h"
#include "drivers/string.h" /* <--- Explicitly include this! */
#include "mem/pmm.h"
#include "mem/paging.h"
#include "fs/tar.h"            
#include "drivers/elf_loader.h"
#include "cpu/task.h" 
#include "fs/vfs.h"
#include "drivers/timer.h"

extern uint32_t global_initrd_address;
int current_exit_code = 0; 

#define MAX_STRESS_PAGES 65536
uint32_t stress_pointers[MAX_STRESS_PAGES];
uint32_t stress_allocated = 0;

uint32_t current_heap_end = 0x40000000; 

char current_process_args[256] = {0};
char current_process_env[256] = "TERM=linux\0USER=root\0";

#define MAX_OPEN_FILES 32
typedef struct {
    uint8_t used;
    vfs_node_t* node;
    uint32_t size;
    uint32_t offset;
} fd_entry_t;

fd_entry_t fd_table[MAX_OPEN_FILES] = {0};

static void build_absolute_path(const char* rel, char* abs) {
    /* Strip leading "./" so the VFS doesn't get confused */
    if (rel[0] == '.' && rel[1] == '/') {
        rel += 2;
    }

    if (rel[0] == '/') {
        strcpy(abs, rel);
    } else {
        strcpy(abs, task_get_cwd());
        if (abs[strlen(abs)-1] != '/') strcat(abs, "/");
        strcat(abs, rel);
    }
}

void syscall_handler(registers_t *regs) {
    switch (regs->eax) {
        case 0: /* SYS_PANIC */
            panic((char*)regs->ebx);
            break;

        case 2: /* SYS_REBOOT */
            reboot();
            break;

        case 3: /* SYS_READ (POSIX read) */
        {
            int fd = (int)regs->ebx;
            char* buf = (char*)regs->ecx;
            int count = (int)regs->edx;
            
            if (fd == 0) { 
                __asm__ volatile("sti");
                vfs_node_t* tty = tty_get_vfs_node();
                uint32_t bytes_read = vfs_read(tty, 0, count, (uint8_t*)buf);
                
                /* Strip the trailing newline that the old gets() used to remove */
                if (bytes_read > 0 && buf[bytes_read - 1] == '\n') {
                    buf[bytes_read - 1] = '\0';
                    bytes_read--;
                } else if (bytes_read < (uint32_t)count) {
                    buf[bytes_read] = '\0';
                } else {
                    buf[count - 1] = '\0';
                }
                
                __asm__ volatile("cli"); 
                regs->eax = bytes_read; 
            } 
            else if (fd >= 3 && fd < MAX_OPEN_FILES && fd_table[fd].used) {
                fd_entry_t* file = &fd_table[fd];
                uint32_t bytes_read = vfs_read(file->node, file->offset, count, (uint8_t*)buf);
                file->offset += bytes_read;
                regs->eax = bytes_read;
            } else {
                regs->eax = -1; 
            }
            break;
        }

        case 4: /* SYS_WRITE (POSIX write) */
        {
            int fd = (int)regs->ebx;
            char* buf = (char*)regs->ecx;
            int count = (int)regs->edx;

            if (fd == 1 || fd == 2) { 
                /* NEW: Write directly to the TTY device */
                vfs_node_t* tty = tty_get_vfs_node();
                uint32_t bytes_written = vfs_write(tty, 0, count, (uint8_t*)buf);
                regs->eax = bytes_written;
            } else {
                regs->eax = -1; 
            }
            break;
        }

        case 5: /* SYS_MEMINFO */
            regs->eax = pmm_get_free_block_count();
            regs->ebx = pmm_get_total_block_count();
            break;

        case 6: /* SYS_EXIT */
        {
            for (int i = 3; i < MAX_OPEN_FILES; i++) {
                fd_table[i].used = 0;
            }
            current_exit_code = regs->ebx; 
            printf("\n[Process %d exited with code %d]\n", current_task, current_exit_code);
            task_exit();
            break;
        }

        case 7: /* SYS_EXEC */
        case 11: /* SYS_SPAWN */
        {
            char* full_cmd = (char*)regs->ebx;
            char filename[64] = {0};
            int i = 0;
            while (full_cmd[i] != ' ' && full_cmd[i] != '\0' && i < 63) {
                filename[i] = full_cmd[i];
                i++;
            }
            int j = 0;
            while (full_cmd[j] != '\0' && j < 255) {
                current_process_args[j] = full_cmd[j];
                j++;
            }
            current_process_args[j] = '\0';

            char abs_path[128];
            build_absolute_path(filename, abs_path);

            vfs_node_t* node = vfs_get_node(abs_path);
            if (!node) {
                printf("Error: File '%s' not found.\n", abs_path);
                regs->eax = -1;
                break;
            }

            uint32_t pages_needed = (node->size + 4095) / 4096;
            uint8_t* temp_buffer = (uint8_t*)pmm_alloc_blocks(pages_needed);
            vfs_read(node, 0, node->size, temp_buffer);

            if (regs->eax == 7) {
                regs->eax = elf_load_and_run(temp_buffer, node->size);
            } else {
                elf_image_t image;
                if (elf_load(temp_buffer, node->size, &image) == 0) {
                    if (create_task((void (*)(void))image.entry, image.page_directory) == 0) regs->eax = 0;
                    else { elf_unload(&image); regs->eax = -1; }
                } else regs->eax = -1;
            }
            
            for (uint32_t p = 0; p < pages_needed; p++) {
                pmm_free_block(temp_buffer + (p * 4096));
            }
            break;
        }

        case 8: /* SYS_LS */
        {
            char* target = (char*)regs->ebx;
            char abs_path[128];
            
            if (target[0] == '\0') strcpy(abs_path, task_get_cwd());
            else build_absolute_path(target, abs_path);
            
            vfs_node_t* target_dir = vfs_get_node(abs_path);
            if (!target_dir) {
                printf("ls: cannot access '%s': No such directory\n", abs_path);
                break;
            }
            
            int index = 0;
            struct dirent *dir_node = 0;
            while ((dir_node = vfs_readdir(target_dir, index)) != NULL) {
                printf("%s ", dir_node->name);
                index++;
            }
            printf("\n");
            break;
        }

        case 9: /* SYS_SLEEP */
            __asm__ volatile("sti"); 
            timer_sleep((uint32_t)regs->ebx);
            __asm__ volatile("cli");
            break;

        case 10: /* SYS_YIELD */
            __asm__ volatile("int $32");
            break;

        case 12: /* SYS_PUTCHAR_AT */
        {
            char c = (char)regs->ebx;
            uint8_t color = (uint8_t)regs->ecx;
            size_t x = (size_t)(regs->edx >> 16);
            size_t y = (size_t)(regs->edx & 0xFFFF);
            terminal_putentryat(c, color, x, y);
            break;
        }

        case 13: /* SYS_STRESS_RAM */
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

        case 14: /* SYS_CLEAR */
            terminal_clear();
            break;

        case 15: /* SYS_BRK */
        {
            int increment = (int)regs->ebx;
            if (increment == 0) {
                regs->eax = current_heap_end; 
            } else {
                uint32_t old_break = current_heap_end;
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
                
                current_heap_end = new_break;
                regs->eax = old_break; 
            }
            break;
        }

        case 16: /* SYS_OPEN */
        {
            char* filename = (char*)regs->ebx;
            char abs_path[128];
            build_absolute_path(filename, abs_path);
            
            vfs_node_t* node = vfs_get_node(abs_path);
            if (!node) { regs->eax = -1; break; }
            
            int fd = -1;
            for (int i = 3; i < MAX_OPEN_FILES; i++) {
                if (fd_table[i].used == 0) {
                    fd_table[i].used = 1;
                    fd_table[i].node = node;
                    fd_table[i].size = node->size;
                    fd_table[i].offset = 0;
                    fd = i;
                    break;
                }
            }
            if (fd != -1) vfs_open(node);
            regs->eax = fd; 
            break;
        }

        case 17: /* SYS_CLOSE */
        {
            int fd = (int)regs->ebx;
            if (fd >= 3 && fd < MAX_OPEN_FILES && fd_table[fd].used) {
                vfs_close(fd_table[fd].node);
                fd_table[fd].used = 0;
                regs->eax = 0; 
            } else {
                regs->eax = -1; 
            }
            break;
        }

        case 18: /* SYS_GET_ARGS */
        {
            char* buf = (char*)regs->ebx;
            int count = 0;
            while (current_process_args[count] != '\0') {
                buf[count] = current_process_args[count];
                count++;
            }
            buf[count] = '\0';
            regs->eax = count;
            break;
        }

        case 19: /* SYS_FORK */
        {
            regs->eax = task_fork(regs);
            break;
        }

        case 20: /* SYS_WAIT */
        {
            task_wait((int)regs->ebx);
            regs->eax = 0;
            break;
        }

        case 21: /* SYS_GET_ENV */
        {
            char* buf = (char*)regs->ebx;
            int count = 0;
            while (!(current_process_env[count] == '\0' && current_process_env[count+1] == '\0') && count < 254) {
                buf[count] = current_process_env[count];
                count++;
            }
            buf[count] = '\0';
            buf[count+1] = '\0';
            regs->eax = count + 1;
            break;
        }

        case 22: /* SYS_CHDIR */
        {
            char* path = (char*)regs->ebx;
            char abs_path[128];
            build_absolute_path(path, abs_path);
            
            vfs_node_t* node = vfs_get_node(abs_path);
            
            if (node && (node->flags & 0x02 || node->flags & 0x08)) {
                task_set_cwd(abs_path);
                regs->eax = 0;
            } else {
                regs->eax = -1;
            }
            break;
        }

        case 23: /* SYS_GETCWD */
        {
            char* buf = (char*)regs->ebx;
            strcpy(buf, task_get_cwd());
            regs->eax = 0;
            break;
        }

        case 24: /* SYS_IOCTL */
        {
            int fd = (int)regs->ebx;
            uint32_t request = regs->ecx;
            void* argp = (void*)regs->edx;

            if (fd == 0 || fd == 1 || fd == 2) {
                /* Standard streams (stdin, stdout, stderr) point directly to the TTY */
                regs->eax = vfs_ioctl(tty_get_vfs_node(), request, argp);
            } 
            else if (fd >= 3 && fd < MAX_OPEN_FILES && fd_table[fd].used) {
                regs->eax = vfs_ioctl(fd_table[fd].node, request, argp);
            } 
            else {
                regs->eax = -1;
            }
            break;
        }

        default:
            printf("IotaOS: Unknown Syscall %d\n", regs->eax);
            break;
    }
}