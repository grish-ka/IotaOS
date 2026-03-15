/*
* IotaOS - iosh.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "iota.h"

/* Helper to print numbers since we don't have printf in app-space yet */
void print_num(uint32_t num) {
    if (num == 0) { iota_print_str(IOTA_RELOC("0")); return; }
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
    iota_print_str(IOTA_RELOC("\nWelcome to Iota Shell (IOSH) v0.1.1!\n"));

    volatile char cmd[256];

    while (1) {
        /* Zero the buffer */
        for(int i = 0; i < 256; i++) ((char*)cmd)[i] = 0;

        iota_print_str(IOTA_RELOC("user@IotaOS$ "));
        iota_get_line((char*)cmd, 256);

        if (iota_strcmp((char*)cmd, IOTA_RELOC("help")) == 0) {
            iota_print_str(IOTA_RELOC("Available commands:\n"));
            iota_print_str(IOTA_RELOC("  help    - Show this help message\n"));
            iota_print_str(IOTA_RELOC("  ls      - List files in the ramdisk\n"));
            iota_print_str(IOTA_RELOC("  run     - Execute an .ib file (e.g. './test.ib')\n"));
            iota_print_str(IOTA_RELOC("  meminfo - Show physical memory usage information\n"));
            iota_print_str(IOTA_RELOC("  version - Show the kernel and shell version\n"));
            iota_print_str(IOTA_RELOC("  clear   - Clear the terminal screen\n"));
            iota_print_str(IOTA_RELOC("  reboot  - Reboot the system\n"));
            iota_print_str(IOTA_RELOC("  exit    - Exit the shell and shutdown the system\n"));
        } 
        else if (iota_strcmp((char*)cmd, IOTA_RELOC("ls")) == 0) {
            iota_ls(); /* Ask kernel to print the files */
        }
        else if (cmd[0] == '.' && cmd[1] == '/') {
            /* Extract filename after "./" */
            char* filename = (char*)&cmd[2]; 
            iota_exec(filename); /* Ask kernel to run it */
        }
        else if (iota_strcmp((char*)cmd, IOTA_RELOC("version")) == 0) {
            iota_print_str(IOTA_RELOC("IotaOS Version: 0.1.3\n"));
            iota_print_str(IOTA_RELOC("IotaOS Kernel Version: 0.1.3\n"));
            iota_print_str(IOTA_RELOC("Iota Shell (IOSH) Version: 0.1.1\n"));
            iota_print_str(IOTA_RELOC("Copyright (c) 2026 grish-ka. Licensed under MIT.\n"));
        }
        else if (iota_strcmp((char*)cmd, IOTA_RELOC("clear")) == 0) {
            iota_clear();
        }
        else if (iota_strcmp((char*)cmd, IOTA_RELOC("reboot")) == 0) {
            iota_print_str(IOTA_RELOC("Rebooting...\n"));
            iota_reboot();
        }
        else if (iota_strcmp((char*)cmd, IOTA_RELOC("exit")) == 0) {
            return 0;
        }
        else if (iota_strcmp((char*)cmd, IOTA_RELOC("bsod")) == 0) {
            __asm__ volatile("int $1");
        }
        else if (iota_strcmp((char*)cmd, IOTA_RELOC("meminfo")) == 0) {
            uint32_t free_blocks, total_blocks;
            iota_meminfo(&free_blocks, &total_blocks);
            
            iota_print_str(IOTA_RELOC("Physical Memory: "));
            print_num((free_blocks * 4096) / 1024 / 1024);
            iota_print_str(IOTA_RELOC(" / "));
            print_num((total_blocks * 4096) / 1024 / 1024);
            iota_print_str(IOTA_RELOC(" MB Free\n"));
        }
        else if (cmd[0] != '\0') {
            iota_print_str(IOTA_RELOC("IOSH: Unknown command: "));
            iota_print_str((char*)cmd);
            iota_print_str(IOTA_RELOC("\n"));
        }
    }
}