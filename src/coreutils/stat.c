/*
* IotaOS - stat.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <sys/stat.h>
#include <time.h>

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    int status = 0;
    if (argc < 2) {
        fprintf(stderr, "%s: missing operand\n", prog);
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) < 0) {
            fprintf(stderr, "%s: cannot statx '%s': %s\n", prog, argv[i], strerror(errno));
            status = 1;
            continue;
        }
        const char *type = S_ISDIR(st.st_mode) ? "directory" : S_ISCHR(st.st_mode) ? "character special file"
                         : S_ISFIFO(st.st_mode) ? "fifo" : st.st_size ? "regular file" : "regular empty file";
        char perms[11];
        perms[0] = S_ISDIR(st.st_mode) ? 'd' : S_ISCHR(st.st_mode) ? 'c' : '-';
        for (int k = 0; k < 9; k++) perms[k + 1] = (st.st_mode & (0400 >> k)) ? "rwxrwxrwx"[k] : '-';
        perms[10] = '\0';
        char when[64];
        time_t t = st.st_mtime;
        struct tm tm;
        localtime_r(&t, &tm);
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S.000000000 +0000", &tm);

        printf("  File: %s\n", argv[i]);
        printf("  Size: %-15ld Blocks: %-10ld IO Block: %-6ld %s\n", st.st_size, st.st_blocks, st.st_blksize, type);
        printf("Device: %xh/%dd\tInode: %-11d Links: %d\n", st.st_dev, st.st_dev, st.st_ino, st.st_nlink);
        printf("Access: (%04o/%s)  Uid: (%5d/    root)   Gid: (%5d/    root)\n", st.st_mode & 07777, perms, st.st_uid, st.st_gid);
        printf("Access: %s\nModify: %s\nChange: %s\n Birth: -\n", when, when, when);
    }
    return status;
}
