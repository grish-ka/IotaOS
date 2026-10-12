#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

typedef struct {
    int fd;
} FILE;

void printf(const char* format, ...);
int sprintf(char *buffer, const char* format, ...);
void puts(const char* str);
void putchar(char c);
void gets(char* buffer, int max_size);

FILE* fopen(const char* filename, const char* mode);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
int fclose(FILE* stream);

#endif