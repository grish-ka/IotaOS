/*
 * IotaOS - iosh.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "iota.h"

int main() {
    printf("\n\033[32mWelcome to Iota Shell (IOSH) v1.0 (VFS Directories)!\033[0m\n");

    char cmd[256];
    char cwd[128];

    while (1) {
        memset(cmd, 0, 256);
        
        iota_getcwd(cwd);
        printf("\033[36muser@IotaOS:%s$ \033[0m", cwd);
        gets(cmd, 256);

        if (cmd[0] == '\0') continue;

        if (strcmp(cmd, "help") == 0) {
            printf("Available commands:\n");
            printf("  cd <dir> - Change working directory\n");
            printf("  ls       - List files in current directory\n");
            printf("  clear    - Clear screen\n");
            printf("  reboot   - Reboot system\n");
            printf("  exit     - Shutdown system\n");
        } 
        else if (cmd[0] == 'c' && cmd[1] == 'd' && (cmd[2] == ' ' || cmd[2] == '\0')) {
            char* target = &cmd[3];
            if (cmd[2] == '\0' || target[0] == '\0') target = "/";
            
            if (iota_chdir(target) != 0) {
                printf("cd: %s: No such file or directory\n", target);
            }
        }
        else if (cmd[0] == 'l' && cmd[1] == 's') {
            char* target = "";
            if (cmd[2] == ' ') target = &cmd[3];
            iota_ls(target); 
        }
        else if (strcmp(cmd, "clear") == 0) iota_clear();
        else if (strcmp(cmd, "reboot") == 0) iota_reboot();
        else if (strcmp(cmd, "exit") == 0) return 0;
        else if (strcmp(cmd, "meminfo") == 0) {
            uint32_t free_b, total_b;
            iota_meminfo(&free_b, &total_b);
            printf("Physical Memory: %d / %d MB Free\n", (free_b*4)/1024, (total_b*4)/1024);
        }
        else {
            int len = strlen(cmd);
            int run_in_background = 0;
            
            if (len > 0 && cmd[len - 1] == '&') {
                run_in_background = 1;
                cmd[len - 1] = '\0';
                len--;
                while (len > 0 && cmd[len - 1] == ' ') {
                    cmd[len - 1] = '\0';
                    len--;
                }
            }

            char exec_path[256];
            int has_slash = 0;
            
            /* Only check for a slash BEFORE the first space */
            for (int i = 0; cmd[i] != '\0' && cmd[i] != ' '; i++) {
                if (cmd[i] == '/') has_slash = 1;
            }

            if (!has_slash) {
                /* Command has no slash, prepend /bin/ */
                sprintf(exec_path, "/bin/%s", cmd);
            } else {
                /* Command has a slash (e.g. ./tests), use exactly as typed */
                strcpy(exec_path, cmd);
            }
            /* ----------------------------------- */

            int pid = iota_fork();

            if (pid == 0) {
                iota_exec(exec_path); 
                printf("IOSH: %s: Command not found or failed.\n", cmd);
                iota_exit(1); 
            } else if (pid > 0) {
                if (run_in_background) printf("[PID %d] Background Task\n", pid);
                else iota_wait(pid);
            } else {
                printf("IOSH Error: fork() failed.\n");
            }
        }
    }
}