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
#include "fs/tar.h"            
#include "drivers/elf_loader.h"
#include "cpu/task.h" 

extern uint32_t global_initrd_address;
int current_exit_code = 0; 

#define MAX_STRESS_PAGES 65536 /* Allows up to 256 MB to be hoarded */
uint32_t stress_pointers[MAX_STRESS_PAGES];
uint32_t stress_allocated = 0;

void syscall_handler(registers_t *regs) {
    switch (regs->eax) {
        case 0: /* SYS_PANIC */
            panic((char*)regs->ebx);
            break;

        case 1: /* SYS_PRINT */
            terminal_writestring((char*)regs->ecx); 
            break;

        case 2: /* SYS_REBOOT */
            reboot();
            break;

        case 3: /* SYS_READ_LINE */
            __asm__ volatile("sti"); 
            gets((char*)regs->ebx, (int)regs->ecx);
            __asm__ volatile("cli"); 
            break;

        case 4: /* SYS_CLEAR */
            terminal_clear();
            break;

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
                        /* Feed the isolated Page Directory into the scheduler! */
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
            /* Unpack the X and Y coordinates from the EDX register */
            size_t x = (size_t)(regs->edx >> 16);
            size_t y = (size_t)(regs->edx & 0xFFFF);
            
            terminal_putentryat(c, color, x, y);
            break;
        }

        case 13: /* SYS_STRESS_RAM */
        {
            if (regs->ebx == 1) { /* Allocate */
                uint32_t pages_to_alloc = regs->ecx * 256; /* 256 pages per MB */
                uint32_t allocated_this_run = 0;
                
                while (allocated_this_run < pages_to_alloc && stress_allocated < MAX_STRESS_PAGES) {
                    // lets see with no limit < MAX_STRESS_PAGES
                    void* block = pmm_alloc_block();
                    if (!block) break; /* Out of memory! */
                    stress_pointers[stress_allocated++] = (uint32_t)block;
                    allocated_this_run++;
                }
            } 
            else if (regs->ebx == 0) { /* Free */
                for (uint32_t i = 0; i < stress_allocated; i++) {
                    pmm_free_block((void*)stress_pointers[i]);
                }
                stress_allocated = 0;
            }
            break;
        }

        default:
            printf("IotaOS: Unknown Syscall %d\n", regs->eax);
            break;
    }
}