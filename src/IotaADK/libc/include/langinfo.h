/*
* IotaOS - langinfo.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _LANGINFO_H
#define _LANGINFO_H

typedef int nl_item;

#define CODESET    14
#define RADIXCHAR  0x10000
#define THOUSEP    0x10001
#define D_T_FMT    0x20028
#define D_FMT      0x20029
#define T_FMT      0x2002A
#define T_FMT_AMPM 0x2002B
#define AM_STR     0x20026
#define PM_STR     0x20027
#define YESEXPR    0x50000
#define NOEXPR     0x50001
#define DAY_1      0x20007
#define ABDAY_1    0x20000
#define MON_1      0x2001A
#define ABMON_1    0x2000E
#define ALTMON_1   0x2006F

char *nl_langinfo(nl_item item);

#endif
