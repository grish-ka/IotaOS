/*
* IotaOS - unistd.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "unistd.h"
#include "signal.h"
#include "iota.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>

/* Kernel calls return -errno on failure */
static int check(int r) {
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return r;
}

ssize_t read(int fd, void *buf, size_t count) {
    return check(iota_syscall3(IOTA_SYS_READ, fd, (uint32_t)buf, count));
}

ssize_t write(int fd, const void *buf, size_t count) {
    return check(iota_syscall3(IOTA_SYS_WRITE, fd, (uint32_t)buf, count));
}

int close(int fd) {
    return check(iota_syscall3(IOTA_SYS_CLOSE, fd, 0, 0));
}

off_t lseek(int fd, off_t offset, int whence) {
    return check(iota_syscall3(IOTA_SYS_LSEEK, fd, (uint32_t)offset, whence));
}

int dup(int oldfd) {
    return check(iota_syscall3(IOTA_SYS_DUP, oldfd, 0, 0));
}

int dup2(int oldfd, int newfd) {
    return check(iota_syscall3(IOTA_SYS_DUP2, oldfd, newfd, 0));
}

int pipe(int pipefd[2]) {
    return check(iota_syscall3(IOTA_SYS_PIPE, (uint32_t)pipefd, 0, 0));
}

int isatty(int fd) {
    struct termios t;
    if (iota_syscall3(IOTA_SYS_IOCTL, fd, TCGETS, (uint32_t)&t) < 0) {
        errno = ENOTTY;
        return 0;
    }
    return 1;
}

char *ttyname(int fd) {
    if (!isatty(fd)) return NULL;
    return "/dev/tty1";
}

int ftruncate(int fd, off_t length) {
    return check(iota_syscall3(IOTA_SYS_FTRUNCATE, fd, (uint32_t)length, 0));
}

int truncate(const char *path, off_t length) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int r = ftruncate(fd, length);
    close(fd);
    return r;
}

int fsync(int fd) {
    (void)fd;
    return 0;
}

int access(const char *pathname, int mode) {
    struct stat st;
    if (stat(pathname, &st) < 0) return -1;
    /* Everyone is root, so only the execute bits can deny access */
    if ((mode & X_OK) && !(st.st_mode & 0111) && !S_ISDIR(st.st_mode)) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

int chdir(const char *path) {
    return check(iota_syscall3(IOTA_SYS_CHDIR, (uint32_t)path, 0, 0));
}

char *getcwd(char *buf, size_t size) {
    char path[256];
    iota_getcwd(path);
    size_t need = strlen(path) + 1;

    if (!buf) {
        if (size == 0 || size < need) size = need;
        buf = malloc(size);
        if (!buf) {
            errno = ENOMEM;
            return NULL;
        }
    } else if (size < need) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buf, path, need);
    return buf;
}

int unlink(const char *pathname) {
    return check(iota_syscall3(IOTA_SYS_UNLINK, (uint32_t)pathname, 0, 0));
}

int rmdir(const char *pathname) {
    return check(iota_syscall3(IOTA_SYS_RMDIR, (uint32_t)pathname, 0, 0));
}

int link(const char *oldpath, const char *newpath) {
    (void)oldpath;
    (void)newpath;
    errno = EPERM;
    return -1;
}

int symlink(const char *target, const char *linkpath) {
    (void)target;
    (void)linkpath;
    errno = EPERM;
    return -1;
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    (void)buf;
    (void)bufsiz;
    struct stat st;
    if (stat(path, &st) < 0) return -1;
    errno = EINVAL; /* nothing is a symlink */
    return -1;
}

int chown(const char *path, uid_t owner, gid_t group) {
    return check(iota_syscall3(IOTA_SYS_CHOWN, (uint32_t)path, owner, group));
}

pid_t getpid(void) {
    return iota_getpid();
}

pid_t getppid(void) {
    return iota_getppid();
}

pid_t fork(void) {
    int pid = iota_fork();
    if (pid < 0) {
        errno = EAGAIN;
        return -1;
    }
    return pid;
}

/* Arguments and environment travel as NUL-separated blocks ending in "" */
static int pack_block(char *out, size_t size, char *const list[]) {
    size_t o = 0;
    for (int i = 0; list && list[i]; i++) {
        size_t len = strlen(list[i]) + 1;
        if (o + len + 1 > size) return -1;
        memcpy(out + o, list[i], len);
        o += len;
    }
    out[o] = '\0';
    if (o == 0) out[1] = '\0';
    return 0;
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    static char args[1024];
    static char env[2048];

    if (pack_block(args, sizeof(args), argv) < 0 || pack_block(env, sizeof(env), envp) < 0) {
        errno = E2BIG;
        return -1;
    }
    /* Only returns on failure */
    return check(iota_syscall3(IOTA_SYS_EXECVE, (uint32_t)pathname, (uint32_t)args, (uint32_t)env));
}

extern char **environ;

int execv(const char *pathname, char *const argv[]) {
    return execve(pathname, argv, environ);
}

int execvp(const char *file, char *const argv[]) {
    if (strchr(file, '/')) return execv(file, argv);

    const char *path = getenv("PATH");
    if (!path) path = "/usr/local/bin:/usr/bin:/bin";
    char candidate[256];
    int saw_eacces = 0;
    while (*path) {
        const char *end = strchr(path, ':');
        size_t len = end ? (size_t)(end - path) : strlen(path);
        if (len + strlen(file) + 2 <= sizeof(candidate)) {
            memcpy(candidate, path, len);
            candidate[len] = '/';
            strcpy(candidate + len + 1, file);
            execv(candidate, argv);
            if (errno == EACCES) saw_eacces = 1;
        }
        if (!end) break;
        path = end + 1;
    }
    errno = saw_eacces ? EACCES : ENOENT;
    return -1;
}

pid_t waitpid(pid_t pid, int *status, int options) {
    int st = 0;
    int r = iota_syscall3(IOTA_SYS_WAITPID, pid, (uint32_t)&st, (options & WNOHANG) != 0);
    if (r < 0) return check(r);
    if (r > 0 && status) *status = st;
    return r;
}

pid_t wait(int *status) {
    return waitpid(-1, status, 0);
}

void _exit(int status) {
    iota_exit(status);
    while(1);
}

static void get_ids(uid_t *uid, gid_t *gid) {
    uint32_t u, g;
    __asm__ volatile ("int $0x80" : "=a"(u), "=b"(g) : "a"(IOTA_SYS_GETIDS) : "memory");
    if (uid) *uid = u;
    if (gid) *gid = g;
}

uid_t getuid(void) { uid_t u; get_ids(&u, NULL); return u; }
gid_t getgid(void) { gid_t g; get_ids(NULL, &g); return g; }
uid_t geteuid(void) { return getuid(); }
gid_t getegid(void) { return getgid(); }

int setuid(uid_t uid) {
    return check(iota_syscall3(IOTA_SYS_SETIDS, uid, getgid(), 0));
}

int setgid(gid_t gid) {
    return check(iota_syscall3(IOTA_SYS_SETIDS, getuid(), gid, 0));
}

/* No job control: every process is in one session and group */
pid_t getpgrp(void) { return 1; }
pid_t setsid(void) { return getpid(); }
int setpgid(pid_t pid, pid_t pgid) { (void)pid; (void)pgid; return 0; }
pid_t tcgetpgrp(int fd) { (void)fd; return 1; }
int tcsetpgrp(int fd, pid_t pgrp) { (void)fd; (void)pgrp; return 0; }

unsigned int sleep(unsigned int seconds) {
    iota_sleep(seconds * 1000);
    return 0;
}

int usleep(unsigned int usec) {
    iota_sleep(usec / 1000 ? usec / 1000 : 1);
    return 0;
}

unsigned int alarm(unsigned int seconds) {
    (void)seconds;
    return 0;
}

int pause(void) {
    for (;;) iota_sleep(1000);
}

int gethostname(char *name, size_t len) {
    if (!name || len == 0) {
        errno = EINVAL;
        return -1;
    }
    const char *fallback = "iotaos";
    size_t n = 0;
    int fd = open("/etc/hostname", O_RDONLY);
    if (fd >= 0) {
        ssize_t r = read(fd, name, len - 1);
        close(fd);
        if (r > 0) n = r;
    }
    while (n > 0 && (name[n - 1] == '\n' || name[n - 1] == ' ')) n--;
    if (n == 0) {
        while (n + 1 < len && fallback[n]) {
            name[n] = fallback[n];
            n++;
        }
    }
    name[n] = '\0';
    return 0;
}

int sethostname(const char *name, size_t len) {
    int fd = open("/etc/hostname", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, name, len);
    write(fd, "\n", 1);
    close(fd);
    return 0;
}

char *getlogin(void) {
    char *user = getenv("LOGNAME");
    return user ? user : "root";
}

long sysconf(int name) {
    switch (name) {
    case _SC_CLK_TCK: return 100;
    case _SC_PAGESIZE: return 4096;
    case _SC_OPEN_MAX: return 32;
    }
    errno = EINVAL;
    return -1;
}

char *optarg = NULL;
int optind = 1, opterr = 1, optopt = 0;

int getopt(int argc, char * const argv[], const char *optstring) {
    static int nextchar = 0;

    if (optind >= argc || !argv[optind]) return -1;
    char *arg = argv[optind];
    if (nextchar == 0) {
        if (arg[0] != '-' || arg[1] == '\0') return -1;
        if (strcmp(arg, "--") == 0) {
            optind++;
            return -1;
        }
        nextchar = 1;
    }

    int c = arg[nextchar++];
    const char *spec = strchr(optstring, c);
    if (!spec || c == ':') {
        optopt = c;
        if (opterr && optstring[0] != ':') fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], c);
        if (arg[nextchar] == '\0') { optind++; nextchar = 0; }
        return '?';
    }

    if (spec[1] == ':') {
        if (arg[nextchar] != '\0') {
            optarg = arg + nextchar;
        } else if (optind + 1 < argc) {
            optarg = argv[++optind];
        } else {
            optopt = c;
            optind++;
            nextchar = 0;
            if (opterr && optstring[0] != ':') fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], c);
            return optstring[0] == ':' ? ':' : '?';
        }
        optind++;
        nextchar = 0;
    } else if (arg[nextchar] == '\0') {
        optind++;
        nextchar = 0;
    }
    return c;
}

/*
 * The kernel has no readiness polling, and all fds block on read. A
 * zero-timeout poll therefore reports nothing pending; any real wait reports
 * the requested read/write fds as ready so the caller's blocking read() can
 * proceed. Timeouts are not honoured.
 */
static int select_common(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, int poll_only) {
    int ready = 0;
    if (exceptfds) FD_ZERO(exceptfds);
    if (poll_only) {
        if (readfds) FD_ZERO(readfds);
        if (writefds) FD_ZERO(writefds);
        return 0;
    }
    for (int fd = 0; fd < nfds && fd < FD_SETSIZE; fd++) {
        if (readfds && FD_ISSET(fd, readfds)) ready++;
        if (writefds && FD_ISSET(fd, writefds)) ready++;
    }
    return ready;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    int poll_only = timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0;
    return select_common(nfds, readfds, writefds, exceptfds, poll_only);
}

int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            const struct timespec *timeout, const sigset_t *sigmask) {
    (void)sigmask;
    int poll_only = timeout && timeout->tv_sec == 0 && timeout->tv_nsec == 0;
    return select_common(nfds, readfds, writefds, exceptfds, poll_only);
}
