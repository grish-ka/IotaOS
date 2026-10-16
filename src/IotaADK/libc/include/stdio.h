/*
* IotaOS - stdio.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h> /* ADD THIS for size_t */
#include <sys/types.h>

#define EOF (-1)

typedef struct _FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int getc(FILE *stream);
int fgetc(FILE *stream);
int getchar(void);
int ungetc(int c, FILE *stream);
char *fgets(char *s, int size, FILE *stream);
char *gets(char *buffer, int max_size); /* Legacy IotaOS form: bounded read */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

int putc(int c, FILE *stream);
int fputc(int c, FILE *stream);
int putchar(int c);
int fputs(const char *s, FILE *stream);
int puts(const char *s);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int fscanf(FILE *stream, const char *format, ...);

FILE *fopen(const char *pathname, const char *mode);
FILE *fdopen(int fd, const char *mode);
int fclose(FILE *stream);
int fflush(FILE *stream);
int fileno(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int rename(const char *oldpath, const char *newpath);
int remove(const char *pathname);
void perror(const char *s);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
void setbuf(FILE *stream, char *buf);
void setlinebuf(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream);
int dprintf(int fd, const char *format, ...);

#define BUFSIZ 1024
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#endif