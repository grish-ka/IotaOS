/*
 * IotaOS - stdio.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

void putchar(char c);
void puts(const char* str);
int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);
char* gets(char* str, int max_length);
char getchar(void);

#endif /* STDIO_H */