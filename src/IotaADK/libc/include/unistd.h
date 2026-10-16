/*
* IotaOS - unistd.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Seek directions */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* File access modes for access() */
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int pipe(int pipefd[2]);
int isatty(int fd);
char *ttyname(int fd);
int ftruncate(int fd, off_t length);
int truncate(const char *path, off_t length);
int fsync(int fd);

int access(const char *pathname, int mode);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
int unlink(const char *pathname);
int rmdir(const char *pathname);
int link(const char *oldpath, const char *newpath);
int symlink(const char *target, const char *linkpath);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
int chown(const char *path, uid_t owner, gid_t group);

pid_t getpid(void);
pid_t getppid(void);
pid_t fork(void);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execv(const char *pathname, char *const argv[]);
int execvp(const char *file, char *const argv[]);
void _exit(int status) __attribute__((noreturn));

uid_t getuid(void);
gid_t getgid(void);
uid_t geteuid(void);
gid_t getegid(void);
int setuid(uid_t uid);
int setgid(gid_t gid);
pid_t getpgrp(void);
pid_t setsid(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);

unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
unsigned int alarm(unsigned int seconds);
int pause(void);

int gethostname(char *name, size_t len);
int sethostname(const char *name, size_t len);
char *getlogin(void);
long sysconf(int name);
#define _SC_CLK_TCK   2
#define _SC_PAGESIZE  30
#define _SC_OPEN_MAX  4

/* getopt(3) */
extern char *optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char * const argv[], const char *optstring);

#endif
