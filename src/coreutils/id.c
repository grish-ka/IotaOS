/*
* IotaOS - id.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <pwd.h>
#include <grp.h>

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int only_user = 0, only_group = 0, names = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'u') only_user = 1;
            else if (*p == 'g' || *p == 'G') only_group = 1;
            else if (*p == 'n') names = 1;
            else {
                fprintf(stderr, "%s: invalid option -- '%c'\n", prog, *p);
                return 1;
            }
        }
    }

    uid_t uid = getuid();
    gid_t gid = getgid();
    if (i < argc) {
        struct passwd *pw = getpwnam(argv[i]);
        if (!pw) {
            fprintf(stderr, "%s: '%s': no such user\n", prog, argv[i]);
            return 1;
        }
        uid = pw->pw_uid;
        gid = pw->pw_gid;
    }
    struct passwd *pw = getpwuid(uid);
    char user[64];
    snprintf(user, sizeof(user), "%s", pw ? pw->pw_name : "?");
    struct group *gr = getgrgid(gid);
    char group[64];
    snprintf(group, sizeof(group), "%s", gr ? gr->gr_name : "root");

    if (only_user) {
        if (names) puts(user);
        else printf("%d\n", uid);
    } else if (only_group) {
        if (names) puts(group);
        else printf("%d\n", gid);
    } else {
        printf("uid=%d(%s) gid=%d(%s) groups=%d(%s)\n", uid, user, gid, group, gid, group);
    }
    return 0;
}
