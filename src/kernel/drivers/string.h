/* 
 * IotaOS - string.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>

int strcmp(const char* str1, const char* str2);
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);
void* memset(void* dest, int val, size_t count);
void* memcpy(void* dest, const void* src, size_t count);

#endif /* STRING_H */