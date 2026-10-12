#include "stdio.h"
#include "iota.h"
#include "string.h"
#include "stdlib.h"

static int int_to_string(int num, int base, char* buffer) {
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }
    int is_negative = 0;
    unsigned int unum = num;
    if (num < 0 && base == 10) {
        is_negative = 1;
        unum = (unsigned int)(-num);
    }
    const char digits[] = "0123456789abcdef";
    char temp[32];
    int temp_i = 0;
    while (unum > 0) {
        temp[temp_i++] = digits[unum % base];
        unum /= base;
    }
    if (is_negative) temp[temp_i++] = '-';
    int count = 0;
    while (temp_i > 0) buffer[count++] = temp[--temp_i];
    buffer[count] = '\0';
    return count;
}

void putchar(char c) {
    iota_write(1, &c, 1);
}

void puts(const char* str) {
    iota_write(1, str, strlen(str));
    iota_write(1, "\n", 1);
}

void gets(char* buffer, int max_size) {
    iota_read(0, buffer, max_size);
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                const char* str = va_arg(args, char*);
                iota_write(1, str, strlen(str));
            } else if (*format == 'd') {
                char buf[32];
                int len = int_to_string(va_arg(args, int), 10, buf);
                iota_write(1, buf, len);
            } else if (*format == 'x') {
                char buf[32];
                int len = int_to_string(va_arg(args, int), 16, buf);
                iota_write(1, buf, len);
            } else if (*format == 'c') {
                putchar((char)va_arg(args, int));
            }
        } else {
            putchar(*format);
        }
        format++;
    }
    va_end(args);
}

int sprintf(char *buffer, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int buf_idx = 0;
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                char *str = va_arg(args, char *);
                while (*str) buffer[buf_idx++] = *str++;
            } else if (*format == 'd') {
                buf_idx += int_to_string(va_arg(args, int), 10, &buffer[buf_idx]);
            } else if (*format == 'x') {
                buf_idx += int_to_string(va_arg(args, int), 16, &buffer[buf_idx]);
            } else if (*format == 'c') {
                buffer[buf_idx++] = (char)va_arg(args, int);
            }
        } else {
            buffer[buf_idx++] = *format;
        }
        format++;
    }
    buffer[buf_idx] = '\0';
    va_end(args);
    return buf_idx;
}

FILE* fopen(const char* filename, const char* mode) {
    /* For now, we ignore the 'mode' string because the TAR ramdisk is read-only */
    int fd = iota_open(filename);
    if (fd < 0) return NULL;
    
    FILE* file = (FILE*)malloc(sizeof(FILE));
    if (!file) {
        iota_close(fd);
        return NULL;
    }
    
    file->fd = fd;
    return file;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!stream || !ptr) return 0;
    
    int bytes_to_read = size * nmemb;
    int bytes_read = iota_read(stream->fd, ptr, bytes_to_read);
    
    if (bytes_read <= 0) return 0;
    return bytes_read / size; /* Return number of items successfully read */
}

int fclose(FILE* stream) {
    if (!stream) return -1;
    
    int res = iota_close(stream->fd);
    free(stream); /* Return the FILE struct memory to the user-space heap */
    
    return res;
}