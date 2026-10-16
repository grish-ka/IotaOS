/*
* IotaOS - login.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <pwd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/utsname.h>

/*
 * A getty-style login: shows /etc/issue, asks for a user name, then starts
 * that user's shell as a login shell in their home directory. There are no
 * passwords: accounts with "x" or "" in /etc/passwd log straight in.
 */

static void show_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    copy_fd(fd, STDOUT_FILENO);
    close(fd);
}

/* /etc/issue escapes: \n hostname, \s system, \r release, \m machine, \l tty, \d \t date/time */
static void show_issue(void) {
    FILE *f = fopen("/etc/issue", "r");
    if (!f) return;
    struct utsname u;
    uname(&u);
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c != '\\') {
            putchar(c);
            continue;
        }
        char buf[64];
        switch (c = fgetc(f)) {
        case 'n': fputs(u.nodename, stdout); break;
        case 's': fputs(u.sysname, stdout); break;
        case 'r': fputs(u.release, stdout); break;
        case 'm': fputs(u.machine, stdout); break;
        case 'l': fputs("tty1", stdout); break;
        case 'd': strftime(buf, sizeof(buf), "%a %b %e %Y", &tm); fputs(buf, stdout); break;
        case 't': strftime(buf, sizeof(buf), "%H:%M:%S", &tm); fputs(buf, stdout); break;
        case EOF: break;
        default: putchar(c); break;
        }
    }
    fclose(f);
}

int main(void) {
    char host[65];
    gethostname(host, sizeof(host));

    fputs("\033[H\033[2J", stdout);
    show_issue();

    struct passwd *pw = NULL;
    while (!pw) {
        printf("%s login: ", host);
        char name[64];
        if (!fgets(name, sizeof(name), stdin)) {
            putchar('\n');
            continue;
        }
        name[strcspn(name, "\n")] = '\0';
        if (!name[0]) continue;

        pw = getpwnam(name);
        if (!pw) {
            sleep(1);
            printf("\nLogin incorrect\n");
        }
    }

    /* Copy what we need: getpwnam's buffer is static */
    uid_t uid = pw->pw_uid;
    gid_t gid = pw->pw_gid;
    char user[64], home[128], shell[128];
    snprintf(user, sizeof(user), "%s", pw->pw_name);
    snprintf(home, sizeof(home), "%s", pw->pw_dir);
    snprintf(shell, sizeof(shell), "%s", pw->pw_shell[0] ? pw->pw_shell : "/bin/sh");

    setgid(gid);
    setuid(uid);

    if (chdir(home) < 0) {
        printf("No directory, logging in with HOME=/\n");
        snprintf(home, sizeof(home), "/");
        chdir("/");
    }

    /* Show when this user last logged in, then record this login */
    char lastlog[192];
    snprintf(lastlog, sizeof(lastlog), "%s/.lastlogin", home);
    int fd = open(lastlog, O_RDONLY);
    if (fd >= 0) {
        char last[64] = {0};
        ssize_t n = read(fd, last, sizeof(last) - 1);
        close(fd);
        if (n > 0) printf("Last login: %s", last);
    }
    fd = open(lastlog, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char stamp[64];
        strftime(stamp, sizeof(stamp), "%a %b %e %H:%M:%S %Y on tty1\n", &tm);
        write(fd, stamp, strlen(stamp));
        close(fd);
    }
    show_file("/etc/motd");

    char e_home[160], e_user[80], e_logname[80], e_shell[160], e_path[128], e_term[32], e_mail[160];
    snprintf(e_home, sizeof(e_home), "HOME=%s", home);
    snprintf(e_user, sizeof(e_user), "USER=%s", user);
    snprintf(e_logname, sizeof(e_logname), "LOGNAME=%s", user);
    snprintf(e_shell, sizeof(e_shell), "SHELL=%s", shell);
    snprintf(e_path, sizeof(e_path), "PATH=%s", uid == 0
             ? "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
             : "/usr/local/bin:/usr/bin:/bin");
    snprintf(e_term, sizeof(e_term), "TERM=linux");
    snprintf(e_mail, sizeof(e_mail), "MAIL=/var/mail/%s", user);
    char *envp[] = {e_home, e_user, e_logname, e_shell, e_path, e_term, e_mail, NULL};

    /* A leading '-' in argv[0] tells the shell it is a login shell */
    const char *base = strrchr(shell, '/');
    char argv0[64];
    snprintf(argv0, sizeof(argv0), "-%s", base ? base + 1 : shell);
    char *argv[] = {argv0, NULL};
    execve(shell, argv, envp);
    fprintf(stderr, "login: cannot execute %s: %s\n", shell, strerror(errno));
    /* Fall back to whatever shell exists */
    argv[0] = "-sh";
    execve("/bin/sh", argv, envp);
    argv[0] = "iosh";
    execve("/bin/iosh", argv, envp);
    return 1;
}
