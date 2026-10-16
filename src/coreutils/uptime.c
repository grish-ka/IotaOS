/*
* IotaOS - uptime.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <time.h>
#include "iota.h"

int main(int argc, char *argv[]) {
    iota_sysinfo_t info;
    iota_syscall3(IOTA_SYS_SYSINFO, (uint32_t)&info, 0, 0);
    unsigned long secs = info.uptime_ticks / info.ticks_per_second;
    unsigned long days = secs / 86400, hours = (secs % 86400) / 3600, mins = (secs % 3600) / 60;

    if (argc > 1 && !strcmp(argv[1], "-p")) {
        printf("up");
        if (days) printf(" %lu day%s,", days, days == 1 ? "" : "s");
        if (hours) printf(" %lu hour%s,", hours, hours == 1 ? "" : "s");
        printf(" %lu minute%s\n", mins, mins == 1 ? "" : "s");
        return 0;
    }

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    printf(" %02d:%02d:%02d up ", tm.tm_hour, tm.tm_min, tm.tm_sec);
    if (days) printf("%lu day%s, ", days, days == 1 ? "" : "s");
    if (hours) printf("%2lu:%02lu", hours, mins);
    else printf("%lu min", mins);
    printf(",  1 user,  load average: 0.00, 0.00, 0.00\n");
    return 0;
}
