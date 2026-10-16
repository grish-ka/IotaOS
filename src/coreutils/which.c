/*
* IotaOS - which.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    const char *path = getenv("PATH");
    if (!path) path = "/usr/local/bin:/usr/bin:/bin";
    int status = 0;
    for (int i = 1; i < argc; i++) {
        int found = 0;
        const char *p = path;
        while (*p && !found) {
            const char *end = strchr(p, ':');
            size_t len = end ? (size_t)(end - p) : strlen(p);
            char candidate[512];
            snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)len, p, argv[i]);
            struct stat st;
            if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & 0111)) {
                puts(candidate);
                found = 1;
            }
            if (!end) break;
            p = end + 1;
        }
        if (!found) status = 1;
    }
    return status;
}
