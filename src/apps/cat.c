/*
 * IotaOS - cat.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: cat <filename>\n");
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        printf("cat: %s: No such file or directory\n", argv[1]);
        return 1;
    }

    char buffer[64];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, 63, file)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    
    printf("\n");
    fclose(file);
    return 0;
}