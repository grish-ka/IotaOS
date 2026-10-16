/*
* IotaOS - hostname.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (sethostname(argv[1], strlen(argv[1])) < 0) {
            warn_errno(prog_name(argv[0]), "cannot set hostname");
            return 1;
        }
        return 0;
    }
    char name[65];
    gethostname(name, sizeof(name));
    puts(name);
    return 0;
}
