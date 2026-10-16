/*
* IotaOS - reboot.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include "iota.h"

/* Installed as reboot, poweroff, halt and shutdown */
int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    (void)argc;
    printf("\nThe system is going down NOW!\n");
    sleep(1);
    if (!strcmp(prog, "reboot")) iota_reboot();
    else iota_syscall3(IOTA_SYS_POWEROFF, 0, 0, 0);
    return 0;
}
