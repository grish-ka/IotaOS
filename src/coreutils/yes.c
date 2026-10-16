/*
* IotaOS - yes.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

int main(int argc, char *argv[]) {
    char line[256] = "y";
    if (argc > 1) {
        line[0] = '\0';
        for (int i = 1; i < argc; i++) {
            if (i > 1) strncat(line, " ", sizeof(line) - strlen(line) - 1);
            strncat(line, argv[i], sizeof(line) - strlen(line) - 1);
        }
    }
    strncat(line, "\n", sizeof(line) - strlen(line) - 1);
    size_t len = strlen(line);
    while (write(STDOUT_FILENO, line, len) > 0) {}
    return 0;
}
