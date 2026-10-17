/*
* IotaOS - inttypes.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _INTTYPES_H
#define _INTTYPES_H

#include <stdint.h>

/* printf/scanf conversions for the <stdint.h> types (i686: long long is 64-bit) */
#define PRId8  "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 "lld"
#define PRIi8  "i"
#define PRIi16 "i"
#define PRIi32 "i"
#define PRIi64 "lli"
#define PRIu8  "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "llu"
#define PRIo8  "o"
#define PRIo16 "o"
#define PRIo32 "o"
#define PRIo64 "llo"
#define PRIx8  "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 "llx"
#define PRIX8  "X"
#define PRIX16 "X"
#define PRIX32 "X"
#define PRIX64 "llX"
#define PRIdMAX "lld"
#define PRIiMAX "lli"
#define PRIuMAX "llu"
#define PRIoMAX "llo"
#define PRIxMAX "llx"
#define PRIXMAX "llX"
#define PRIdPTR "d"
#define PRIuPTR "u"
#define PRIxPTR "x"
#define SCNd32 "d"
#define SCNd64 "lld"
#define SCNu32 "u"
#define SCNu64 "llu"
#define SCNx32 "x"
#define SCNx64 "llx"
#define SCNdMAX "lld"
#define SCNuMAX "llu"

intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);

#endif
