/*
* IotaOS - chown.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <pwd.h>
#include <grp.h>

/* chown USER[:GROUP] FILE... */
int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    if (argc < 3) {
        fprintf(stderr, "Usage: %s OWNER[:GROUP] FILE...\n", prog);
        return 1;
    }
    char spec[128];
    snprintf(spec, sizeof(spec), "%s", argv[1]);
    char *group = strchr(spec, ':');
    if (group) *group++ = '\0';

    uid_t uid = (uid_t)-1;
    gid_t gid = (gid_t)-1;
    if (spec[0]) {
        struct passwd *pw = getpwnam(spec);
        if (pw) uid = pw->pw_uid;
        else if (spec[0] >= '0' && spec[0] <= '9') uid = atoi(spec);
        else {
            fprintf(stderr, "%s: invalid user: '%s'\n", prog, argv[1]);
            return 1;
        }
    }
    if (group && group[0]) {
        struct group *gr = getgrnam(group);
        if (gr) gid = gr->gr_gid;
        else if (group[0] >= '0' && group[0] <= '9') gid = atoi(group);
        else {
            fprintf(stderr, "%s: invalid group: '%s'\n", prog, group);
            return 1;
        }
    }

    int status = 0;
    for (int i = 2; i < argc; i++) {
        if (chown(argv[i], uid, gid) < 0) {
            fprintf(stderr, "%s: changing ownership of '%s': %s\n", prog, argv[i], strerror(errno));
            status = 1;
        }
    }
    return status;
}
