/* * IotaOS - iosh.c (The IotaOS
 Shell)
 * Ported from kernel_main to Userland
 * Copyright (c) 2026 grish-ka
 */

#include "iota.h"

// We need a way to read input from Userland. 
// For now, we'll assume the kernel provides a SYS_READ (3) or similar.
// Since you don't have a full stdio yet, we'll use a local helper.

void main() {
    iota_print("\nWelcome to Iota Shell (IOSH)!\n", 31);
    char cmd[256];

    while (1) {
        iota_print("user@IotaOS$ ", 13);
        
        // IMPORTANT: You'll need to implement a SYS_READ_LINE syscall
        // to get input from the keyboard driver back up to this app.
        iota_get_line(cmd, 256);
        
        if (iota_strcmp(cmd, "help") == 0) {
            iota_print("Available commands:\n", 20);
            iota_print("  help    - Show this help\n", 27);
            iota_print("  ls      - List ramdisk files\n", 31);
            iota_print("  clear   - Clear screen\n", 25);
            iota_print("  reboot  - Reboot IotaOS\n", 26);
        } 
        else if (iota_strcmp(cmd, "clear") == 0) {
            // Syscall for clearing the screen
            iota_clear(); 
        }
        else if (iota_strcmp(cmd, "reboot") == 0) {
            iota_reboot();
        }
        else if (cmd[0] != '\0') {
            iota_print("IOSH: Unknown command\n", 22);
        }
    }
}