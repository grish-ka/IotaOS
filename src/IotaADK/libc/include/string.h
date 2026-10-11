#ifndef _STRING_H
#define _STRING_H
#include <stdint.h>
#include <stddef.h>

int strcmp(const char* s1, const char* s2);
size_t strlen(const char* str);
void* memset(void* ptr, int value, size_t num);
#endif