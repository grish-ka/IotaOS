/*
* IotaOS - ps.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include "iota.h"
#include <pwd.h>

/* ps            PID TTY TIME CMD (like procps)
   ps aux / -ef  full listing with parent and state */
int main(int argc, char *argv[]) {
    int full = 0;
    for (int i = 1; i < argc; i++) {
        if (strchr(argv[i], 'a') || strchr(argv[i], 'e') || strchr(argv[i], 'f') || strchr(argv[i], 'x')) full = 1;
    }

    iota_proc_t procs[64];
    int n = iota_syscall3(IOTA_SYS_PROCLIST, (uint32_t)procs, 64, 0);
    iota_sysinfo_t info;
    iota_syscall3(IOTA_SYS_SYSINFO, (uint32_t)&info, 0, 0);

    if (full) printf("USER       PID  PPID STAT  START   TIME COMMAND\n");
    else printf("  PID TTY          TIME CMD\n");

    for (int i = 0; i < n; i++) {
        if (procs[i].state == 'Z' && !full) continue;
        unsigned long secs = procs[i].cpu_ticks / info.ticks_per_second;
        char elapsed[16];
        snprintf(elapsed, sizeof(elapsed), "%lu:%02lu", secs / 60, secs % 60);
        if (full) {
            unsigned long started = procs[i].start_tick / info.ticks_per_second;
            struct passwd *pw = getpwuid(procs[i].uid);
            printf("%-8.8s %5d %5d %-4c %3lu:%02lu %6s %s\n", pw ? pw->pw_name : "?", procs[i].pid, procs[i].ppid,
                   procs[i].state, started / 60, started % 60, elapsed, procs[i].name);
        } else {
            printf("%5d tty1     %02lu:%02lu:%02lu %s\n", procs[i].pid,
                   secs / 3600, (secs / 60) % 60, secs % 60, procs[i].name);
        }
    }
    return 0;
}
