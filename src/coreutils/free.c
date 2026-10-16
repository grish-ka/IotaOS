/*
* IotaOS - free.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include "iota.h"

int main(int argc, char *argv[]) {
    int unit = 1024; /* -k (default), -m, -g, -b; -h for human-readable */
    int human = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--mega")) unit = 1024 * 1024;
        else if (!strcmp(argv[i], "-g")) unit = 1024 * 1024 * 1024;
        else if (!strcmp(argv[i], "-b")) unit = 1;
        else if (!strcmp(argv[i], "-h")) human = 1;
    }

    iota_sysinfo_t info;
    iota_syscall3(IOTA_SYS_SYSINFO, (uint32_t)&info, 0, 0);
    unsigned long total = (unsigned long)info.total_pages * 4096;
    unsigned long avail = (unsigned long)info.free_pages * 4096;
    unsigned long used = total - avail;

    printf("%7s%12s%12s%12s%12s%12s%12s\n", "", "total", "used", "free", "shared", "buff/cache", "available");
    if (human) {
        char t[16], u[16], f[16];
        human_size(total, t, sizeof(t));
        human_size(used, u, sizeof(u));
        human_size(avail, f, sizeof(f));
        printf("%-7s%12s%12s%12s%12s%12s%12s\n", "Mem:", t, u, f, "0B", "0B", f);
        printf("%-7s%12s%12s%12s\n", "Swap:", "0B", "0B", "0B");
    } else {
        printf("%-7s%12lu%12lu%12lu%12lu%12lu%12lu\n", "Mem:",
               total / unit, used / unit, avail / unit, 0UL, 0UL, avail / unit);
        printf("%-7s%12lu%12lu%12lu\n", "Swap:", 0UL, 0UL, 0UL);
    }
    return 0;
}
