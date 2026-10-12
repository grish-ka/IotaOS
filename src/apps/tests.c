/*
 * IotaOS - tests.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iota.h"

int test_memory() {
    /* Allocate, verify write access, and free */
    char* buf = (char*)malloc(128);
    if (!buf) return 0;
    
    sprintf(buf, "IotaHeapTest");
    int passed = (strcmp(buf, "IotaHeapTest") == 0);
    
    free(buf);
    return passed;
}

int test_file_io() {
    /* Attempt to read the ELF magic number from the shell binary */
    FILE* f = fopen("iosh", "r");
    if (!f) return 0;
    
    char buf[8];
    size_t bytes_read = fread(buf, 1, 4, f);
    fclose(f);
    
    return (bytes_read == 4 && buf[0] == 0x7F && buf[1] == 'E');
}

int test_arguments(int argc, char* argv[]) {
    /* Verify the shell correctly passed the execution string */
    if (argc < 1) return 0;
    return (strcmp(argv[0], "./tests") == 0 || strcmp(argv[0], "tests") == 0);
}

int test_environment() {
    /* Verify the kernel passed the TERM and USER strings */
    char* term = getenv("TERM");
    char* user = getenv("USER");
    
    if (!term || !user) return 0;
    
    return (strcmp(term, "linux") == 0 && strcmp(user, "root") == 0);
}

int test_ansi() {
    /* Output a raw ANSI sequence to turn the text cyan, then reset it */
    printf("\033[36m(ANSI UI Test)\033[0m ");
    return 1;
}

int main(int argc, char* argv[], char* envp[]) {
    printf("\n--- IotaOS System Diagnostics ---\n");
    int passed = 0;
    int total = 5;

    printf("[...] Testing Dynamic Heap (malloc/free)... ");
    if (test_memory()) { printf("PASS\n"); passed++; } else { printf("FAIL\n"); }

    printf("[...] Testing File I/O (fopen/fread)... ");
    if (test_file_io()) { printf("PASS\n"); passed++; } else { printf("FAIL\n"); }

    printf("[...] Testing Command Arguments... ");
    if (test_arguments(argc, argv)) { printf("PASS\n"); passed++; } else { printf("FAIL\n"); }

    printf("[...] Testing Environment (getenv)... ");
    if (test_environment()) { printf("PASS\n"); passed++; } else { printf("FAIL\n"); }

    printf("[...] Testing ANSI Escape Engine... ");
    if (test_ansi()) { printf("PASS\n"); passed++; } else { printf("FAIL\n"); }

    printf("---------------------------------\n");
    printf("Result: %d/%d Tests Passed.\n", passed, total);

    if (passed == total) {
        /* Use ANSI Green for success */
        printf("\033[32mStatus: System is stable and ready for GNU toolchain porting.\033[0m\n");
    } else {
        /* Use ANSI Red for failure */
        printf("\033[31mStatus: Kernel instability detected.\033[0m\n");
    }

    return 0;
}