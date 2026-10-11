/*
 * IotaOS - syscall.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "cpu/isr.h"
#include "drivers/stdio.h"
#include "drivers/terminal.h"
#include "drivers/system.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "fs/tar.h"            
#include "drivers/elf_loader.h"
#include "cpu/task.h" 

extern uint32_t global_initrd_address;
int current_exit_code = 0; 

#define MAX_STRESS_PAGES 65536
uint32_t stress_pointers[MAX_STRESS_PAGES];
uint32_t stress_allocated = 0;

uint32_t current_heap_end = 0x40000000; /* Start the heap at the 1GB virtual mark */

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
                gets(buf, count); 
                __asm__ volatile("cli"); 
                regs->eax = count; 
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
                for (int i = 0; i < count; i++) {
                    terminal_putchar(buf[i]);
                }
                regs->eax = count;
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
            current_exit_code = regs->ebx; 
            printf("\n[Process exited with code %d]\n", current_exit_code);
            break;

        case 7: /* SYS_EXEC */
        {
            char* filename = (char*)regs->ebx;
            if (global_initrd_address == 0) {
                printf("Error: No ramdisk loaded.\n");
                regs->eax = -1;
            } else {
                uint32_t file_size = 0;
                void* file_data = tar_get_file(global_initrd_address, filename, &file_size);
                if (file_data == NULL) {
                    printf("Error: File '%s' not found.\n", filename);
                    regs->eax = -1;
                } else {
                    regs->eax = elf_load_and_run(file_data, file_size);
                }
            }
            break;
        }

        case 8: /* SYS_LS */
            if (global_initrd_address == 0) {
                printf("Error: No ramdisk loaded.\n");
            } else {
                tar_parse(global_initrd_address);
            }
            break;

        case 9: /* SYS_SLEEP */
            __asm__ volatile("sti"); 
            sleep((uint32_t)regs->ebx);
            __asm__ volatile("cli");
            break;

        case 10: /* SYS_YIELD */
            __asm__ volatile("int $32");
            break;

        case 11: /* SYS_SPAWN (BACKGROUND TASK) */
        {
            char* filename = (char*)regs->ebx;
            if (global_initrd_address == 0) {
                regs->eax = -1;
            } else {
                uint32_t file_size = 0;
                void* file_data = tar_get_file(global_initrd_address, filename, &file_size);
                if (file_data == NULL) {
                    printf("Error: File '%s' not found.\n", filename);
                    regs->eax = -1;
                } else {
                    elf_image_t image;
                    if (elf_load(file_data, file_size, &image) == 0) {
                        if (create_task((void (*)(void))image.entry, image.page_directory) == 0) {
                            regs->eax = 0;
                        } else {
                            elf_unload(&image);
                            regs->eax = -1;
                        }
                    } else {
                        regs->eax = -1;
                    }
                }
            }
            break;
        }

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

        case 15: /* SYS_BRK (Memory Allocation) */
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

        default:
            printf("IotaOS: Unknown Syscall %d\n", regs->eax);
            break;
    }
}