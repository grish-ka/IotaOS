#ifndef _STDIO_H
#define _STDIO_H
#include <stdarg.h>

void printf(const char* format, ...);
int sprintf(char *buffer, const char* format, ...);
void puts(const char* str);
void putchar(char c);
void gets(char* buffer, int max_size);
#endif