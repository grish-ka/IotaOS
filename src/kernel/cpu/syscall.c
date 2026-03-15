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
#include "drivers/ib_loader.h" 
#include "cpu/task.h" /* <--- ADD THIS FOR create_task() */

extern uint32_t global_initrd_address;
int current_exit_code = 0; 

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
            } else {
                void* file_data = tar_get_file(global_initrd_address, filename);
                if (file_data == NULL) {
                    printf("Error: File '%s' not found.\n", filename);
                } else {
                    ib_load_and_run(file_data);
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
                void* file_data = tar_get_file(global_initrd_address, filename);
                if (file_data == NULL) {
                    printf("Error: File '%s' not found.\n", filename);
                    regs->eax = -1;
                } else {
                    struct ib_header* header = (struct ib_header*)file_data;
                    if (header->magic == IB_MAGIC) {
                        /* Get the memory address of the code and send it to the scheduler! */
                        uint32_t code_ptr = (uint32_t)file_data + header->code_offset;
                        create_task((void (*)())code_ptr);
                        regs->eax = 0;
                    } else {
                        printf("Error: Invalid Magic!\n");
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

        default:
            printf("IotaOS: Unknown Syscall %d\n", regs->eax);
            break;
    }
}