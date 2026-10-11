/*

IotaOS - stdio.h

Copyright (c) 2026 grish-ka

Licensed under the MIT License.
*/

#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stdint.h>

void printf(const char* format, ...);
int sprintf(char* buffer, const char* format, ...);

void gets(char* buffer, int max_size);
int atoi(const char* str);
void sleep(uint32_t ms);

/* --- NEW: SERIAL DEBUGGING SYSTEM --- */
void init_serial(void);
void serial_printf(const char* format, ...);

#endif /* STDIO_H */