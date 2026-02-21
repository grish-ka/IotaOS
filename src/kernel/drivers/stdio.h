#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h> /* <-- ADD THIS FOR PRINTF */

void printf(const char* format, ...);
int sprintf(char* buffer, const char* format, ...);

void gets(char* buffer, int max_size);
int atoi(const char* str);

#endif /* STDIO_H */