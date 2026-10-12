/*
 * IotaOS - cat.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>

int main() {
    char filename[64];
    memset(filename, 0, 64);

    printf("File to read: ");
    gets(filename, 64);

    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("cat: %s: No such file or directory\n", filename);
        return 1;
    }

    char buffer[64];
    size_t bytes_read;
    
    /* Read the file in chunks and print it */
    while ((bytes_read = fread(buffer, 1, 63, file)) > 0) {
        buffer[bytes_read] = '\0'; /* Null-terminate the chunk */
        printf("%s", buffer);
    }
    
    printf("\n");
    fclose(file);
    return 0;
}