/*

IotaOS - stdio.c

Copyright (c) 2026 grish-ka

Licensed under the MIT License.
*/

#include "stdio.h"
#include "terminal.h"
#include "keyboard.h"
#include "timer.h"
#include "io.h" /* Needed for COM1 serial ports */
#include <stdint.h>
#include <stddef.h>

/* --- NEW: SERIAL DEBUGGER (COM1) --- */
void init_serial() {
outb(0x3F8 + 1, 0x00);    /* Disable all interrupts */
outb(0x3F8 + 3, 0x80);    /* Enable DLAB (set baud rate divisor) */
outb(0x3F8 + 0, 0x03);    /* Set divisor to 3 (lo byte) 38400 baud */
outb(0x3F8 + 1, 0x00);    /*                  (hi byte) */
outb(0x3F8 + 3, 0x03);    /* 8 bits, no parity, one stop bit */
outb(0x3F8 + 2, 0xC7);    /* Enable FIFO, clear them, with 14-byte threshold */
outb(0x3F8 + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
}

static void serial_putchar(char a) {
while ((inb(0x3F8 + 5) & 0x20) == 0); /* Wait for transmit to be empty */
outb(0x3F8, a);
}

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

/* Sends invisible logs straight to your Linux Terminal! */
void serial_printf(const char *format, ...) {
va_list args;
va_start(args, format);
while (*format != '\0') {
if (*format == '%') {
format++;
if (*format == 's') {
char *str = va_arg(args, char *);
while (*str) serial_putchar(*str++);
} else if (*format == 'd') {
char buf[32];
int_to_string(va_arg(args, int), 10, buf);
char *str = buf;
while (*str) serial_putchar(*str++);
} else if (*format == 'x') {
char buf[32];
int_to_string(va_arg(args, int), 16, buf);
char *str = buf;
while (*str) serial_putchar(*str++);
} else if (*format == 'c') {
serial_putchar((char)va_arg(args, int));
}
} else {
serial_putchar(*format);
}
format++;
}
va_end(args);
}

/* ... existing sprintf, printf, atoi, gets, sleep functions stay below here ... */
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
} else if (*format == '%') {
buffer[buf_idx++] = '%';
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

void printf(const char* format, ...) {
va_list args;
va_start(args, format);
while (*format != '\0') {
if (*format == '%') {
format++;
if (*format == 's') {
terminal_writestring(va_arg(args, char *));
} else if (*format == 'd') {
char buf[32];
int_to_string(va_arg(args, int), 10, buf);
terminal_writestring(buf);
} else if (*format == 'x') {
char buf[32];
int_to_string(va_arg(args, int), 16, buf);
terminal_writestring(buf);
} else if (*format == 'c') {
terminal_putchar((char)va_arg(args, int));
}
} else {
terminal_putchar(*format);
}
format++;
}
va_end(args);
}

int atoi(const char* str) {
int result = 0;
int sign = 1;
while (*str == ' ') str++;
if (*str == '-') { sign = -1; str++; }
while (*str >= '0' && *str <= '9') {
result = result * 10 + (*str - '0');
str++;
}
return result * sign;
}

void gets(char* buffer, int max_size) {
int count = 0;
while (count < max_size - 1) {
char c = getchar();
if (c == '\n') {
printf("\n");
break;
} else if (c == '\b') {
if (count > 0) {
count--;
printf("\b");
}
} else if (c != 0 && c != '\r') {
buffer[count++] = c;
printf("%c", c);
}
}
buffer[count] = '\0';
}

void sleep(uint32_t ms) {
uint32_t target_ticks = timer_get_ticks() + (ms / 10);
if (ms > 0 && ms < 10) target_ticks = timer_get_ticks() + 1;
while (timer_get_ticks() < target_ticks) {
asm volatile("hlt");
}
}