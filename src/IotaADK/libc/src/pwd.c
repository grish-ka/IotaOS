/*
* IotaOS - pwd.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <pwd.h>
#include <grp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/* Read a whole small file (/etc/passwd, /etc/group) into buf */
static int slurp(const char *path, char *buf, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    size_t n = 0;
    ssize_t r;
    while (n + 1 < size && (r = read(fd, buf + n, size - 1 - n)) > 0) n += r;
    close(fd);
    buf[n] = '\0';
    return 0;
}

/* Split line at ':' into at most max fields, in place */
static int split_fields(char *line, char **fields, int max) {
    int n = 0;
    fields[n++] = line;
    for (char *p = line; *p && n < max; p++) {
        if (*p == ':') {
            *p = '\0';
            fields[n++] = p + 1;
        }
    }
    return n;
}

static char passwd_buf[2048];
static char *passwd_next = NULL;
static struct passwd pw_entry;

static struct passwd root_pwd = {
    "root", "x", 0, 0, "root", "/root", "/bin/bash"
};

void setpwent(void) {
    if (slurp("/etc/passwd", passwd_buf, sizeof(passwd_buf)) < 0) passwd_buf[0] = '\0';
    passwd_next = passwd_buf;
}

void endpwent(void) {
    passwd_next = NULL;
}

struct passwd *getpwent(void) {
    if (!passwd_next) setpwent();
    while (*passwd_next) {
        char *line = passwd_next;
        char *nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
            passwd_next = nl + 1;
        } else {
            passwd_next = line + strlen(line);
        }
        if (line[0] == '#' || line[0] == '\0') continue;

        char *f[7];
        if (split_fields(line, f, 7) < 7) continue;
        pw_entry.pw_name = f[0];
        pw_entry.pw_passwd = f[1];
        pw_entry.pw_uid = atoi(f[2]);
        pw_entry.pw_gid = atoi(f[3]);
        pw_entry.pw_gecos = f[4];
        pw_entry.pw_dir = f[5];
        pw_entry.pw_shell = f[6];
        return &pw_entry;
    }
    return NULL;
}

struct passwd *getpwuid(uid_t uid) {
    struct passwd *p;
    setpwent();
    while ((p = getpwent()) != NULL) {
        if (p->pw_uid == uid) return p;
    }
    return uid == 0 ? &root_pwd : NULL;
}

struct passwd *getpwnam(const char *name) {
    struct passwd *p;
    setpwent();
    while ((p = getpwent()) != NULL) {
        if (strcmp(p->pw_name, name) == 0) return p;
    }
    return strcmp(name, "root") == 0 ? &root_pwd : NULL;
}

static char group_buf[1024];
static char *group_next = NULL;
static struct group gr_entry;
static char *gr_members[16];

void setgrent(void) {
    if (slurp("/etc/group", group_buf, sizeof(group_buf)) < 0) group_buf[0] = '\0';
    group_next = group_buf;
}

void endgrent(void) {
    group_next = NULL;
}

struct group *getgrent(void) {
    if (!group_next) setgrent();
    while (*group_next) {
        char *line = group_next;
        char *nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
            group_next = nl + 1;
        } else {
            group_next = line + strlen(line);
        }
        if (line[0] == '#' || line[0] == '\0') continue;

        char *f[4];
        if (split_fields(line, f, 4) < 4) continue;
        gr_entry.gr_name = f[0];
        gr_entry.gr_passwd = f[1];
        gr_entry.gr_gid = atoi(f[2]);
        int n = 0;
        char *m = f[3];
        while (*m && n < 15) {
            gr_members[n++] = m;
            char *comma = strchr(m, ',');
            if (!comma) break;
            *comma = '\0';
            m = comma + 1;
        }
        gr_members[n] = NULL;
        gr_entry.gr_mem = gr_members;
        return &gr_entry;
    }
    return NULL;
}

struct group *getgrgid(gid_t gid) {
    struct group *g;
    setgrent();
    while ((g = getgrent()) != NULL) {
        if (g->gr_gid == gid) return g;
    }
    return NULL;
}

struct group *getgrnam(const char *name) {
    struct group *g;
    setgrent();
    while ((g = getgrent()) != NULL) {
        if (strcmp(g->gr_name, name) == 0) return g;
    }
    return NULL;
}

int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups) {
    (void)user;
    int room = *ngroups;
    *ngroups = 1;
    if (room < 1) return -1;
    groups[0] = group;
    return 1;
}
