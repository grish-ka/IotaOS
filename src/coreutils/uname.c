/*
* IotaOS - uname.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <sys/utsname.h>

int main(int argc, char *argv[]) {
    int s = 0, n = 0, r = 0, v = 0, m = 0, o = 0;
    for (int i = 1; i < argc; i++) {
        for (char *p = argv[i] + 1; argv[i][0] == '-' && *p; p++) {
            switch (*p) {
            case 'a': s = n = r = v = m = o = 1; break;
            case 's': s = 1; break;
            case 'n': n = 1; break;
            case 'r': r = 1; break;
            case 'v': v = 1; break;
            case 'm': case 'p': case 'i': m = 1; break;
            case 'o': o = 1; break;
            }
        }
    }
    if (!(s | n | r | v | m | o)) s = 1;

    struct utsname u;
    uname(&u);
    const char *parts[6];
    int count = 0;
    if (s) parts[count++] = u.sysname;
    if (n) parts[count++] = u.nodename;
    if (r) parts[count++] = u.release;
    if (v) parts[count++] = u.version;
    if (m) parts[count++] = u.machine;
    if (o) parts[count++] = "IotaOS";
    for (int i = 0; i < count; i++) printf("%s%s", i ? " " : "", parts[i]);
    putchar('\n');
    return 0;
}
