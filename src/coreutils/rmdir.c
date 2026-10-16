/*
* IotaOS - rmdir.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int status = 0;
    if (argc < 2) {
        fprintf(stderr, "%s: missing operand\n", prog);
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (rmdir(argv[i]) < 0) {
            fprintf(stderr, "%s: failed to remove '%s': %s\n", prog, argv[i], strerror(errno));
            status = 1;
        }
    }
    return status;
}
