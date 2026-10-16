/*
* IotaOS - ioctl.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <stdarg.h>

int ioctl(int fd, unsigned long request, ...) {
    void* argp = 0;
    va_list ap;
    va_start(ap, request);
    argp = va_arg(ap, void*);
    va_end(ap);

    int ret;
    __asm__ volatile("int $0x80" 
                     : "=a"(ret) 
                     : "a"(24), "b"(fd), "c"(request), "d"(argp));
    return ret;
}