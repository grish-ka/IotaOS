/*
* IotaOS - locale.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "locale.h"
#include <string.h>

/* The standard minimum POSIX fallback values */
static struct lconv default_locale = {
    ".", "", "", "", "", "", "", "", "", "",
    127, 127, 127, 127, 127, 127, 127, 127
};

/* Only the "C" (a.k.a. "POSIX") locale exists; "" (from the environment)
   selects it too, and any other name is refused */
char* setlocale(int category, const char* locale) {
    (void)category;
    if (!locale || !*locale || !strcmp(locale, "C") || !strcmp(locale, "POSIX") ||
        !strncmp(locale, "C.", 2)) {
        return "C";
    }
    return NULL;
}

struct lconv* localeconv(void) {
    return &default_locale;
}