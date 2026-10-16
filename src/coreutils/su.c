/*
* IotaOS - su.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <pwd.h>

extern char **environ;

/* su [-|-l] [USER]: start USER's shell (root by default). There are no
   passwords, so this only refuses unknown users. */
int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int login = 0, i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-") || !strcmp(argv[i], "-l") || !strcmp(argv[i], "--login")) login = 1;
    }
    const char *name = (i < argc) ? argv[i] : "root";
    struct passwd *pw = getpwnam(name);
    if (!pw) {
        fprintf(stderr, "%s: user %s does not exist or the user entry does not contain all the required fields\n", prog, name);
        return 1;
    }
    char shell[128], home[128], user[64];
    snprintf(shell, sizeof(shell), "%s", pw->pw_shell[0] ? pw->pw_shell : "/bin/sh");
    snprintf(home, sizeof(home), "%s", pw->pw_dir);
    snprintf(user, sizeof(user), "%s", pw->pw_name);

    if (setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0) {
        fprintf(stderr, "%s: Authentication failure\n", prog);
        return 1;
    }
    setenv("HOME", home, 1);
    setenv("SHELL", shell, 1);
    if (login || strcmp(name, "root")) {
        setenv("USER", user, 1);
        setenv("LOGNAME", user, 1);
    }
    if (login) chdir(home);

    const char *base = strrchr(shell, '/');
    base = base ? base + 1 : shell;
    char argv0[64];
    snprintf(argv0, sizeof(argv0), "%s%s", login ? "-" : "", base);
    char *args[] = {argv0, NULL};
    execve(shell, args, environ);
    fprintf(stderr, "%s: failed to execute %s: %s\n", prog, shell, strerror(errno));
    return 1;
}
