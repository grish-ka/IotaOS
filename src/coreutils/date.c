/*
* IotaOS - date.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <time.h>

/* date [-u] [-d @SECONDS] [+FORMAT] */
int main(int argc, char *argv[]) {
    const char *format = "%a %b %e %H:%M:%S %Z %Y";
    time_t t = time(NULL);
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '+') format = argv[i] + 1;
        else if (!strcmp(argv[i], "-d") && i + 1 < argc && argv[i + 1][0] == '@') t = atol(argv[++i] + 1);
        else if (!strcmp(argv[i], "-R")) format = "%a, %d %b %Y %H:%M:%S %z";
        else if (!strcmp(argv[i], "-I")) format = "%Y-%m-%d";
    }
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[256];
    strftime(buf, sizeof(buf), format, &tm);
    puts(buf);
    return 0;
}
