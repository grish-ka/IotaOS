/*
* IotaOS - df.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include "iota.h"

/* The root filesystem lives in RAM, so its capacity is the machine's memory */
int main(int argc, char *argv[]) {
    int human = argc > 1 && !strcmp(argv[1], "-h");
    iota_sysinfo_t info;
    iota_syscall3(IOTA_SYS_SYSINFO, (uint32_t)&info, 0, 0);
    unsigned long total = (unsigned long)info.total_pages * 4096;
    unsigned long avail = (unsigned long)info.free_pages * 4096;
    unsigned long used = total - avail;
    int pct = total ? (int)((used * 100 + total - 1) / total) : 0;

    if (human) {
        char t[16], u[16], a[16];
        human_size(total, t, sizeof(t));
        human_size(used, u, sizeof(u));
        human_size(avail, a, sizeof(a));
        printf("Filesystem      Size  Used Avail Use%% Mounted on\n");
        printf("rootfs     %9s %5s %5s %3d%% /\n", t, u, a, pct);
        printf("devtmpfs   %9s %5s %5s %3d%% /dev\n", "0", "0", "0", 0);
    } else {
        printf("Filesystem     1K-blocks    Used Available Use%% Mounted on\n");
        printf("rootfs        %10lu %7lu %9lu %3d%% /\n", total / 1024, used / 1024, avail / 1024, pct);
        printf("devtmpfs      %10d %7d %9d %3d%% /dev\n", 0, 0, 0, 0);
    }
    return 0;
}
