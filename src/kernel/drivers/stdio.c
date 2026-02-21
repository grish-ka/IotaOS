#include "stdio.h"
#include "terminal.h" /* So printf can talk to the screen */
#include <stdint.h>
#include <stddef.h>
#include "keyboard.h"

/* Helper: Converts a number to a string and puts it in the buffer.
   Returns the number of characters written. */
static int int_to_string(int num, int base, char* buffer) {
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    int is_negative = 0;
    unsigned int unum = num; /* Use unsigned for safe hex division */

    if (num < 0 && base == 10) {
        is_negative = 1;
        unum = (unsigned int)(-num);
    }

    const char digits[] = "0123456789abcdef";
    char temp[32];
    int temp_i = 0;

    /* Extract digits backwards */
    while (unum > 0) {
        temp[temp_i++] = digits[unum % base];
        unum /= base;
    }

    if (is_negative) {
        temp[temp_i++] = '-';
    }

    /* Reverse them into the final buffer */
    int count = 0;
    while (temp_i > 0) {
        buffer[count++] = temp[--temp_i];
    }
    buffer[count] = '\0';
    return count;
}

/* Fills 'buffer' with formatted text */
int sprintf(char* buffer, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    int buf_idx = 0;

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            
            if (*format == 's') {
                char* str = va_arg(args, char*);
                while (*str) {
                    buffer[buf_idx++] = *str++;
                }
            } 
            else if (*format == 'd') {
                int num = va_arg(args, int);
                buf_idx += int_to_string(num, 10, &buffer[buf_idx]);
            } 
            else if (*format == 'x') {
                int num = va_arg(args, int);
                buf_idx += int_to_string(num, 16, &buffer[buf_idx]);
            } 
            else if (*format == 'c') {
                char c = (char) va_arg(args, int);
                buffer[buf_idx++] = c;
            } 
            else if (*format == '%') {
                buffer[buf_idx++] = '%';
            }
        } else {
            buffer[buf_idx++] = *format;
        }
        format++;
    }
    
    buffer[buf_idx] = '\0'; /* Null-terminate the string */
    va_end(args);
    return buf_idx;
}

/* Prints formatted text directly to the VGA terminal */
void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            
            if (*format == 's') {
                char* str = va_arg(args, char*);
                terminal_writestring(str);
            } 
            else if (*format == 'd') {
                int num = va_arg(args, int);
                char buf[32];
                int_to_string(num, 10, buf);
                terminal_writestring(buf);
            } 
            else if (*format == 'x') {
                int num = va_arg(args, int);
                char buf[32];
                int_to_string(num, 16, buf);
                terminal_writestring(buf);
            } 
            else if (*format == 'c') {
                char c = (char) va_arg(args, int);
                terminal_putchar(c);
            } 
            else if (*format == '%') {
                terminal_putchar('%');
            }
        } else {
            terminal_putchar(*format);
        }
        format++;
    }
    va_end(args);
}

/* Converts a string of numbers into an actual integer */
int atoi(const char* str) {
    int result = 0;
    int sign = 1;

    /* Skip spaces */
    while (*str == ' ') {
        str++;
    }

    /* Check for negative numbers */
    if (*str == '-') {
        sign = -1;
        str++;
    }

    /* Convert characters to math */
    while (*str >= '0' && *str <= '9') {
        /* Multiply current result by 10 and add the new digit.
           We subtract '0' (ASCII 48) to get the real math number. */
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

/* Reads a line of text from the keyboard into a buffer */
void gets(char* buffer, int max_size) {
    int count = 0;
    
    while (count < max_size - 1) {
        char c = getchar();
        
        if (c == '\n') {
            printf("\n");
            break; /* Enter key pressed, stop reading! */
        } 
        else if (c == '\b') {
            /* Handle Backspace */
            if (count > 0) {
                count--;
                /* Send a backspace to the terminal to erase it visually */
                printf("\b"); 
            }
        } 
        else if (c != 0) {
            /* Normal character: save it and print it so the user can see it */
            buffer[count++] = c;
            printf("%c", c);
        }
    }
    
    /* Always cap off strings with a null terminator! */
    buffer[count] = '\0'; 
}