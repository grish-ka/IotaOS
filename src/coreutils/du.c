/*
* IotaOS - du.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <dirent.h>
#include <sys/stat.h>

static int summarize, human, all;

static void show(unsigned long bytes, const char *path) {
    if (human) {
        char buf[16];
        human_size(bytes, buf, sizeof(buf));
        printf("%s\t%s\n", buf, path);
    } else {
        printf("%lu\t%s\n", (bytes + 1023) / 1024, path);
    }
}

static unsigned long du(const char *path, int depth) {
    struct stat st;
    if (lstat(path, &st) < 0) {
        warn_errno("du", path);
        return 0;
    }
    unsigned long total = st.st_size;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        struct dirent *de;
        while (d && (de = readdir(d)) != NULL) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            char child[512];
            path_join(child, sizeof(child), path, de->d_name);
            total += du(child, depth + 1);
        }
        if (d) closedir(d);
        if (!summarize || depth == 0) show(total, path);
    } else if ((all && !summarize) || depth == 0) {
        show(total, path);
    }
    return total;
}

int main(int argc, char *argv[]) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 's') summarize = 1;
            else if (*p == 'h') human = 1;
            else if (*p == 'a') all = 1;
        }
    }
    if (i >= argc) du(".", 0);
    for (; i < argc; i++) du(argv[i], 0);
    return 0;
}
