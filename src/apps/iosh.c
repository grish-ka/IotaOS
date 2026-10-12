/*
 * IotaOS - iosh.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "iota.h"

int main() {
    printf("\nWelcome to Iota Shell (IOSH) v0.5.0 (POSIX Forking)!\n");

    char cmd[256];

    while (1) {
        memset(cmd, 0, 256);

        printf("user@IotaOS$ ");
        gets(cmd, 256);

        if (strcmp(cmd, "help") == 0) {
            printf("Available commands:\n");
            printf("  help    - Show this help message\n");
            printf("  ls      - List files in the ramdisk\n");
            printf("  run     - Execute in foreground (e.g. './cat test.txt')\n");
            printf("  bg      - Execute in background (e.g. './spinner &')\n");
            printf("  meminfo - Show physical memory usage information\n");
            printf("  clear   - Clear the terminal screen\n");
            printf("  reboot  - Reboot the system\n");
            printf("  exit    - Exit the shell and shutdown the system\n");
        } 
        else if (strcmp(cmd, "ls") == 0) {
            iota_ls(); 
        }
        else if (cmd[0] == '.' && cmd[1] == '/') {
            char* full_command = &cmd[2]; 
            int len = strlen(full_command);
            int run_in_background = 0;
            
            if (len > 0 && full_command[len - 1] == '&') {
                run_in_background = 1;
                full_command[len - 1] = '\0';
                len--;
                while (len > 0 && full_command[len - 1] == ' ') {
                    full_command[len - 1] = '\0';
                    len--;
                }
            }

            /* --- THE REAL POSIX SHELL MAGIC --- */
            int pid = iota_fork();

            if (pid == 0) {
                /* We are the child process. Transform into the new app! */
                iota_exec(full_command); 
                printf("IOSH Error: Command not found.\n");
                iota_exit(1); 
            } else if (pid > 0) {
                /* We are the parent shell. */
                if (run_in_background) {
                    printf("Spawning background task (PID %d)...\n", pid);
                } else {
                    /* Wait for the child to finish before showing the prompt again */
                    iota_wait(pid);
                }
            } else {
                printf("IOSH Error: fork() failed.\n");
            }
        }
        else if (strcmp(cmd, "clear") == 0) iota_clear();
        else if (strcmp(cmd, "reboot") == 0) iota_reboot();
        else if (strcmp(cmd, "exit") == 0) return 0;
        else if (strcmp(cmd, "meminfo") == 0) {
            uint32_t free_blocks, total_blocks;
            iota_meminfo(&free_blocks, &total_blocks);
            printf("Physical Memory: %d / %d MB Free\n", 
                   (free_blocks * 4096) / 1024 / 1024, 
                   (total_blocks * 4096) / 1024 / 1024);
        }
        else if (cmd[0] != '\0') {
            printf("IOSH: Unknown command: %s\n", cmd);
        }
    }
}