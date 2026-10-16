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
void* memset(void* ptr, int value, size_t num);
int strncmp(const char* s1, const char* s2, size_t n);
char* strerror(int errnum); /* NEW */
void* memcpy(void* dest, const void* src, size_t n);
char* strchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
void *memmove(void *dest, const void *src, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
char *strrchr(const char *s, int c);
char *strpbrk(const char *s, const char *accept);
char *strdup(const char *s);
void *memchr(const void *s, int c, size_t n);
int strcoll(const char *s1, const char *s2);
int memcmp(const void *s1, const void *s2, size_t n);
char *strndup(const char *s, size_t n);
char *strncat(char *dest, const char *src, size_t n);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strtok(char *str, const char *delim);
char *strtok_r(char *str, const char *delim, char **saveptr);
char *strsep(char **stringp, const char *delim);
size_t strnlen(const char *s, size_t maxlen);
char *strsignal(int sig);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
char *strcasestr(const char *haystack, const char *needle);
char *strchrnul(const char *s, int c);
size_t strxfrm(char *dest, const char *src, size_t n);
void *mempcpy(void *dest, const void *src, size_t n);
char *stpcpy(char *dest, const char *src);

#endif /* STRING_H */