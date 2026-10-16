/*
* IotaOS - env.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

extern char **environ;

/* env [-i] [NAME=VALUE]... [COMMAND [ARG]...]  (also installed as printenv) */
int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);

    if (!strcmp(prog, "printenv")) {
        if (argc == 1) {
            for (char **e = environ; *e; e++) puts(*e);
            return 0;
        }
        int status = 0;
        for (int i = 1; i < argc; i++) {
            const char *v = getenv(argv[i]);
            if (v) puts(v);
            else status = 1;
        }
        return status;
    }

    int i = 1;
    if (i < argc && (!strcmp(argv[i], "-i") || !strcmp(argv[i], "-"))) {
        clearenv();
        i++;
    }
    for (; i < argc && strchr(argv[i], '='); i++) putenv(argv[i]);
    if (i >= argc) {
        for (char **e = environ; *e; e++) puts(*e);
        return 0;
    }
    execvp(argv[i], argv + i);
    fprintf(stderr, "%s: '%s': %s\n", prog, argv[i], strerror(errno));
    return errno == ENOENT ? 127 : 126;
}
