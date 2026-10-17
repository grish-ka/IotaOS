/*
* IotaOS - limits.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _LIMITS_H
#define _LIMITS_H

/* i686: char is signed, int and long are 32 bits, long long is 64 */
#define CHAR_BIT   8
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  127
#define UCHAR_MAX  255
#define CHAR_MIN   SCHAR_MIN
#define CHAR_MAX   SCHAR_MAX
#define SHRT_MIN   (-32767 - 1)
#define SHRT_MAX   32767
#define USHRT_MAX  65535
#define INT_MIN    (-2147483647 - 1)
#define INT_MAX    2147483647
#define UINT_MAX   4294967295U
#define LONG_MIN   (-2147483647L - 1)
#define LONG_MAX   2147483647L
#define ULONG_MAX  4294967295UL
#define LLONG_MIN  (-9223372036854775807LL - 1)
#define LLONG_MAX  9223372036854775807LL
#define ULLONG_MAX 18446744073709551615ULL
#define SSIZE_MAX  LONG_MAX
#define MB_LEN_MAX 1

/* IotaOS limits */
#define PATH_MAX     256
#define NAME_MAX     63
#define PIPE_BUF     4096
#define ARG_MAX      1024
#define OPEN_MAX     32
#define CHILD_MAX    31
#define LINE_MAX     2048
#define NGROUPS_MAX  1
#define LOGIN_NAME_MAX 32
#define HOST_NAME_MAX  64
#define TTY_NAME_MAX   32
#define IOV_MAX      16
#define LINK_MAX     127
#define MAX_CANON    1024
#define MAX_INPUT    1024
#define PAGESIZE     4096
#define PAGE_SIZE    4096
#define NZERO        20

/* Minimum values every POSIX system guarantees */
#define _POSIX_ARG_MAX       4096
#define _POSIX_CHILD_MAX     25
#define _POSIX_HOST_NAME_MAX 255
#define _POSIX_LINK_MAX      8
#define _POSIX_LOGIN_NAME_MAX 9
#define _POSIX_MAX_CANON     255
#define _POSIX_MAX_INPUT     255
#define _POSIX_NAME_MAX      14
#define _POSIX_NGROUPS_MAX   8
#define _POSIX_OPEN_MAX      20
#define _POSIX_PATH_MAX      256
#define _POSIX_PIPE_BUF      512
#define _POSIX_SSIZE_MAX     32767
#define _POSIX_STREAM_MAX    8
#define _POSIX_TTY_NAME_MAX  9
#define _POSIX_TZNAME_MAX    6
#define _POSIX_SYMLOOP_MAX   8
#define _POSIX_RE_DUP_MAX    255
#define _POSIX_CLOCKRES_MIN  20000000
#define STREAM_MAX           16
#define TZNAME_MAX           6
#define SYMLOOP_MAX          8
#define RE_DUP_MAX           255
#define BC_BASE_MAX          99
#define BC_DIM_MAX           2048
#define BC_SCALE_MAX         99
#define BC_STRING_MAX        1000
#define COLL_WEIGHTS_MAX     2
#define EXPR_NEST_MAX        32
#define CHARCLASS_NAME_MAX   14

#endif
