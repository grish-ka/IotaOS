/*
* IotaOS - stdio_ext.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*
* glibc's stream introspection functions. IotaOS streams are unbuffered, so
* nothing is ever pending; only a pushed-back (ungetc) byte can be buffered.
*/

#ifndef _STDIO_EXT_H
#define _STDIO_EXT_H

#include <stdio.h>

#define FSETLOCKING_QUERY    0
#define FSETLOCKING_INTERNAL 1
#define FSETLOCKING_BYCALLER 2

size_t __fbufsize(FILE *stream);
size_t __fpending(FILE *stream);
int __flbf(FILE *stream);
int __freadable(FILE *stream);
int __fwritable(FILE *stream);
int __freading(FILE *stream);
int __fwriting(FILE *stream);
int __fsetlocking(FILE *stream, int type);
void __fpurge(FILE *stream);
void __fseterr(FILE *stream);
void _flushlbf(void);
size_t __freadahead(FILE *stream);
const char *__freadptr(FILE *stream, size_t *sizep);
void __freadptrinc(FILE *stream, size_t increment);

#endif
