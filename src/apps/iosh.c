/*
 * IotaOS - iosh.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "iota.h"

void print_num(uint32_t num) {
    if (num == 0) { iota_print_str("0"); return; }
    char buf[16];
    int i = 14;
    buf[15] = '\0';
    while(num > 0) {
        buf[i--] = '0' + (num % 10);
        num /= 10;
    }
    iota_print_str(&buf[i+1]);
}

int main() {
    iota_print_str("\nWelcome to Iota Shell (IOSH) v0.1.3 (ELF Native)!\n");

    volatile char cmd[256];

    while (1) {
        for(int i = 0; i < 256; i++) ((char*)cmd)[i] = 0;

        iota_print_str("user@IotaOS$ ");
        iota_get_line((char*)cmd, 256);

        if (iota_strcmp((char*)cmd, "help") == 0) {
            iota_print_str("Available commands:\n");
            iota_print_str("  help    - Show this help message\n");
            iota_print_str("  ls      - List files in the ramdisk\n");
            iota_print_str("  run     - Execute in foreground (e.g. './test.elf')\n");
            iota_print_str("  bg      - Execute in background (e.g. 'bg spinner.elf')\n");
            iota_print_str("  meminfo - Show physical memory usage information\n");
            iota_print_str("  version - Show the kernel and shell version\n");
            iota_print_str("  clear   - Clear the terminal screen\n");
            iota_print_str("  reboot  - Reboot the system\n");
            iota_print_str("  exit    - Exit the shell and shutdown the system\n");
        } 
        else if (iota_strcmp((char*)cmd, "ls") == 0) {
            iota_ls(); 
        }
        else if (cmd[0] == '.' && cmd[1] == '/') {
            char* filename = (char*)&cmd[2]; 
            
            /* Check if the user wants to run this in the background */
            int len = iota_strlen(filename);
            int run_in_background = 0;
            
            /* If the last character is '&', strip it and set the flag */
            if (len > 2 && filename[len - 1] == '&') {
                run_in_background = 1;
                filename[len - 1] = '\0'; /* Remove the '&' */
                
                /* Remove any trailing spaces before the '&' */
                len--;
                while (len > 0 && filename[len - 1] == ' ') {
                    filename[len - 1] = '\0';
                    len--;
                }
            }
            if (run_in_background) {
                iota_print_str("Spawning background task...\n");
                int ret = iota_spawn(filename);
                if (ret == -1) {
                    iota_print_str("IOSH Error: Failed to spawn task in background.\n");
                }
            } else if (!run_in_background) {
                /* Normal Foreground Execution */
                int ret = iota_exec(filename); 
                if (ret == -1) {
                    iota_print_str("IOSH Error: File not found in ramdisk.\n");
                }
            }
        }
        else if (iota_strcmp((char*)cmd, "clear") == 0) {
            iota_clear();
        }
        else if (iota_strcmp((char*)cmd, "reboot") == 0) {
            iota_reboot();
        }
        else if (iota_strcmp((char*)cmd, "exit") == 0) {
            return 0;
        }
        else if (iota_strcmp((char*)cmd, "bsod") == 0) {
            __asm__ volatile("int $1");
        }
        else if (iota_strcmp((char*)cmd, "meminfo") == 0) {
            uint32_t free_blocks, total_blocks;
            iota_meminfo(&free_blocks, &total_blocks);
            
            iota_print_str("Physical Memory: ");
            print_num((free_blocks * 4096) / 1024 / 1024);
            iota_print_str(" / ");
            print_num((total_blocks * 4096) / 1024 / 1024);
            iota_print_str(" MB Free\n");
        }
        else if (cmd[0] != '\0') {
            iota_print_str("IOSH: Unknown command: ");
            iota_print_str((char*)cmd);
            iota_print_str("\n");
        }
    }
}