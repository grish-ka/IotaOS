/*
* IotaOS - socket.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*
* No networking: every socket call fails with EAFNOSUPPORT (or EBADF).
*/

#include <sys/socket.h>
#include <errno.h>

#define FAIL(e) do { errno = (e); return -1; } while (0)

int socket(int d, int t, int p) { (void)d; (void)t; (void)p; FAIL(EAFNOSUPPORT); }
int socketpair(int d, int t, int p, int sv[2]) { (void)d; (void)t; (void)p; (void)sv; FAIL(EAFNOSUPPORT); }
int bind(int fd, const struct sockaddr *a, socklen_t l) { (void)fd; (void)a; (void)l; FAIL(ENOTSOCK); }
int connect(int fd, const struct sockaddr *a, socklen_t l) { (void)fd; (void)a; (void)l; FAIL(ENOTSOCK); }
int listen(int fd, int b) { (void)fd; (void)b; FAIL(ENOTSOCK); }
int accept(int fd, struct sockaddr *a, socklen_t *l) { (void)fd; (void)a; (void)l; FAIL(ENOTSOCK); }
int shutdown(int fd, int h) { (void)fd; (void)h; FAIL(ENOTSOCK); }
ssize_t send(int fd, const void *b, size_t n, int f) { (void)fd; (void)b; (void)n; (void)f; FAIL(ENOTSOCK); }
ssize_t recv(int fd, void *b, size_t n, int f) { (void)fd; (void)b; (void)n; (void)f; FAIL(ENOTSOCK); }
ssize_t sendto(int fd, const void *b, size_t n, int f, const struct sockaddr *t, socklen_t tl) {
    (void)fd; (void)b; (void)n; (void)f; (void)t; (void)tl; FAIL(ENOTSOCK);
}
ssize_t recvfrom(int fd, void *b, size_t n, int f, struct sockaddr *fr, socklen_t *fl) {
    (void)fd; (void)b; (void)n; (void)f; (void)fr; (void)fl; FAIL(ENOTSOCK);
}
ssize_t sendmsg(int fd, const struct msghdr *m, int f) { (void)fd; (void)m; (void)f; FAIL(ENOTSOCK); }
ssize_t recvmsg(int fd, struct msghdr *m, int f) { (void)fd; (void)m; (void)f; FAIL(ENOTSOCK); }
int getsockopt(int fd, int lv, int n, void *v, socklen_t *l) { (void)fd; (void)lv; (void)n; (void)v; (void)l; FAIL(ENOTSOCK); }
int setsockopt(int fd, int lv, int n, const void *v, socklen_t l) { (void)fd; (void)lv; (void)n; (void)v; (void)l; FAIL(ENOTSOCK); }
int getsockname(int fd, struct sockaddr *a, socklen_t *l) { (void)fd; (void)a; (void)l; FAIL(ENOTSOCK); }
int getpeername(int fd, struct sockaddr *a, socklen_t *l) { (void)fd; (void)a; (void)l; FAIL(ENOTSOCK); }

#include <netdb.h>
#include <stddef.h>

int h_errno;

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    (void)node; (void)service; (void)hints;
    *res = NULL;
    return EAI_FAIL;
}

void freeaddrinfo(struct addrinfo *res) { (void)res; }

const char *gai_strerror(int errcode) {
    (void)errcode;
    return "Networking is not available on IotaOS";
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen,
                char *serv, socklen_t servlen, int flags) {
    (void)sa; (void)salen; (void)host; (void)hostlen; (void)serv; (void)servlen; (void)flags;
    return EAI_FAIL;
}

struct hostent *gethostbyname(const char *name) {
    (void)name;
    h_errno = HOST_NOT_FOUND;
    return NULL;
}

struct servent *getservbyname(const char *name, const char *proto) {
    (void)name; (void)proto;
    return NULL;
}
