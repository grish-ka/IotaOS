/*
* IotaOS - locale.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "locale.h"

/* The standard minimum POSIX fallback values */
static struct lconv default_locale = {
    ".", "", "", "", "", "", "", "", "", "",
    127, 127, 127, 127, 127, 127, 127, 127
};

char* setlocale(int category, const char* locale) {
    (void)category;
    (void)locale;
    /* Hardcode the default "C" locale for all requests */
    return "C"; 
}

struct lconv* localeconv(void) {
    return &default_locale;
}