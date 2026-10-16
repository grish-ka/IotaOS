/*
* IotaOS - sudo.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <pwd.h>

extern char **environ;

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    const char *target = "root";
    int i = 1;
    if (i + 1 < argc && !strcmp(argv[i], "-u")) {
        target = argv[i + 1];
        i += 2;
    }
    if (i < argc && !strcmp(argv[i], "-i")) {
        argv[i] = "-bash";
    }
    if (i >= argc) {
        fprintf(stderr, "usage: %s [-u user] command [arg ...]\n", prog);
        return 1;
    }
    struct passwd *pw = getpwnam(target);
    if (!pw) {
        fprintf(stderr, "%s: unknown user %s\n", prog, target);
        return 1;
    }
    if (setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0) {
        fprintf(stderr, "%s: %s: %s (is %s installed set-user-ID root?)\n", prog, target, strerror(errno), prog);
        return 1;
    }
    setenv("SUDO_USER", getenv("USER") ? getenv("USER") : "root", 1);
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("HOME", pw->pw_dir, 1);

    if (!strcmp(argv[i], "-bash")) {
        char *shell[] = {"-bash", NULL};
        execve("/bin/bash", shell, environ);
    } else {
        execvp(argv[i], argv + i);
    }
    fprintf(stderr, "%s: %s: command not found\n", prog, argv[i]);
    return 1;
}
