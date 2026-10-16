/*
* IotaOS - inttypes.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _INTTYPES_H
#define _INTTYPES_H

#include <stdint.h>

intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);

#endif
