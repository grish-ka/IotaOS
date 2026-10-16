/*
* IotaOS - utsname.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <sys/utsname.h>
#include <string.h>
#include <unistd.h>

int uname(struct utsname *buf) {
    strcpy(buf->sysname, "IotaOS");
    gethostname(buf->nodename, sizeof(buf->nodename));
    strcpy(buf->release, "1.0.0-iota");
    strcpy(buf->version, "#1 " __DATE__);
    strcpy(buf->machine, "i686");
    return 0;
}
