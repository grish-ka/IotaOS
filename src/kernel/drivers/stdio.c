/*
 * IotaOS - stdio.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "drivers/stdio.h"
#include "drivers/terminal.h"
#include <stdarg.h>
#include <stdbool.h>

/* Helper function for number to string conversion */
static void itoa(unsigned int value, char* str, int base) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    unsigned int tmp_value;

    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }

    while (value != 0) {
        tmp_value = value % base;
        *ptr++ = (tmp_value < 10) ? (tmp_value + '0') : (tmp_value - 10 + 'a');
        value /= base;
    }
    *ptr = '\0';

    ptr--;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

void putchar(char c) {
    terminal_putchar(c);
}

void puts(const char* str) {
    while (*str) {
        putchar(*str++);
    }
    putchar('\n');
}

int printf(const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    int written = 0;
    char buffer[32];

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            if (format[i] == 'd' || format[i] == 'i') {
                int val = va_arg(arg, int);
                if (val < 0) {
                    putchar('-');
                    written++;
                    val = -val;
                }
                itoa(val, buffer, 10);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    putchar(buffer[j]);
                    written++;
                }
            } else if (format[i] == 'u') {
                unsigned int val = va_arg(arg, unsigned int);
                itoa(val, buffer, 10);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    putchar(buffer[j]);
                    written++;
                }
            } else if (format[i] == 'x' || format[i] == 'X') {
                unsigned int val = va_arg(arg, unsigned int);
                itoa(val, buffer, 16);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    putchar(buffer[j]);
                    written++;
                }
            } else if (format[i] == 's') {
                char* str = va_arg(arg, char*);
                if (!str) str = "(null)";
                for (int j = 0; str[j] != '\0'; j++) {
                    putchar(str[j]);
                    written++;
                }
            } else if (format[i] == 'c') {
                char c = (char)va_arg(arg, int);
                putchar(c);
                written++;
            } else if (format[i] == '%') {
                putchar('%');
                written++;
            }
        } else {
            putchar(format[i]);
            written++;
        }
    }
    va_end(arg);
    terminal_flush_cursor();
    return written;
}

int sprintf(char* str, const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    int written = 0;
    char buffer[32];

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            if (format[i] == 'd' || format[i] == 'i') {
                int val = va_arg(arg, int);
                if (val < 0) {
                    str[written++] = '-';
                    val = -val;
                }
                itoa(val, buffer, 10);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    str[written++] = buffer[j];
                }
            } else if (format[i] == 'u') {
                unsigned int val = va_arg(arg, unsigned int);
                itoa(val, buffer, 10);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    str[written++] = buffer[j];
                }
            } else if (format[i] == 'x' || format[i] == 'X') {
                unsigned int val = va_arg(arg, unsigned int);
                itoa(val, buffer, 16);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    str[written++] = buffer[j];
                }
            } else if (format[i] == 's') {
                char* s = va_arg(arg, char*);
                if (!s) s = "(null)";
                for (int j = 0; s[j] != '\0'; j++) {
                    str[written++] = s[j];
                }
            } else if (format[i] == 'c') {
                char c = (char)va_arg(arg, int);
                str[written++] = c;
            } else if (format[i] == '%') {
                str[written++] = '%';
            }
        } else {
            str[written++] = format[i];
        }
    }
    str[written] = '\0';
    va_end(arg);
    return written;
}

char* gets(char* str, int max_length) {
    int i = 0;
    while (i < max_length - 1) {
        char c = getchar();
        
        if (c == '\n') {
            break;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
            }
        } else {
            str[i++] = c;
        }
    }
    str[i] = '\0';
    return str;
}