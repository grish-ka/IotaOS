/*
* IotaOS - xargs.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <ctype.h>
#include <sys/wait.h>

/* xargs [-n MAX] [COMMAND [ARG]...]: run COMMAND with words from stdin */
static int run(char **argv) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("xargs: fork");
        return 1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "xargs: %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

int main(int argc, char *argv[]) {
    int max_args = 0, i = 1;
    if (i + 1 < argc && !strcmp(argv[i], "-n")) {
        max_args = atoi(argv[i + 1]);
        i += 2;
    }
    char *base_default[] = {"echo"};
    char **base = (i < argc) ? argv + i : base_default;
    int nbase = (i < argc) ? argc - i : 1;

    int cap = nbase + 256;
    char **args = malloc(sizeof(char *) * (cap + 1));
    int n = 0, status = 0;
    for (int k = 0; k < nbase; k++) args[n++] = base[k];

    char word[1024];
    int len = 0, c;
    do {
        c = getchar();
        if (c == EOF || isspace(c)) {
            if (len == 0) continue;
            word[len] = '\0';
            len = 0;
            args[n++] = strdup(word);
            if ((max_args && n - nbase >= max_args) || n >= cap) {
                args[n] = NULL;
                status |= run(args);
                n = nbase;
            }
        } else if (len < (int)sizeof(word) - 1) {
            word[len++] = (char)c;
        }
    } while (c != EOF);

    if (n > nbase) {
        args[n] = NULL;
        status |= run(args);
    }
    return status ? 123 : 0;
}
