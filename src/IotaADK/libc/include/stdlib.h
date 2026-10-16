/*
* IotaOS - stdlib.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifndef MB_CUR_MAX
#define MB_CUR_MAX 1
#endif

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t num, size_t size);
void abort(void) __attribute__((noreturn));
void exit(int status) __attribute__((noreturn));
char *getenv(const char *name);
extern char **environ;

#define RAND_MAX 0x7fff
int rand(void);
void srand(unsigned int seed);
char *mktemp(char *template);
int atoi(const char *nptr);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
double strtod(const char *nptr, char **endptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long long strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
long atol(const char *nptr);
long long atoll(const char *nptr);
double atof(const char *nptr);
int abs(int j);
long labs(long j);
long long llabs(long long j);
int atexit(void (*function)(void));
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int putenv(char *string);
int clearenv(void);
int system(const char *command);
char *realpath(const char *path, char *resolved_path);
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif