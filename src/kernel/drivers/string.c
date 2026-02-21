#include "string.h"

/* Returns 0 if strings are exactly the same */
int strcmp(const char* str1, const char* str2) {
    while (*str1 != '\0' && *str2 != '\0') {
        if (*str1 != *str2) {
            return 1; /* They don't match! */
        }
        str1++;
        str2++;
    }
    /* If both reached the end at the same time, they match */
    if (*str1 == '\0' && *str2 == '\0') {
        return 0; 
    }
    return 1;
}