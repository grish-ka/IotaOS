/*
* IotaOS - whoami.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <pwd.h>

int main(void) {
    struct passwd *pw = getpwuid(geteuid());
    puts(pw ? pw->pw_name : "root");
    return 0;
}
