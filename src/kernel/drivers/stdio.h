/* 
 * IotaOS - stdio.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h> /* <-- ADD THIS FOR PRINTF */
#include <stdint.h>

void printf(const char* format, ...);
int sprintf(char* buffer, const char* format, ...);

void gets(char* buffer, int max_size);
int atoi(const char* str);

/* Pauses execution for a specified number of milliseconds */
void sleep(uint32_t ms);

#endif /* STDIO_H */