/*
* IotaOS - inet.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#include <stdint.h>

/* i686 is little-endian; network order is big-endian */
static inline uint32_t htonl(uint32_t x) { return __builtin_bswap32(x); }
static inline uint32_t ntohl(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t htons(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }
static inline uint16_t ntohs(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }

#endif
