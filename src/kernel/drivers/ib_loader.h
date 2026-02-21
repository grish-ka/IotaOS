/* 
 * IotaOS - ib_loader.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef IB_LOADER_H
#define IB_LOADER_H

#include <stdint.h>

// TODO: Add ib binary stuff

#define IB_MAGIC 0x4249 /* 'I' and 'B' in little-endian */

/* The header that sits at the very beginning of every .ib file */
struct ib_header {
    uint16_t magic;         /* Must be 0x4249 */
    uint16_t version;       /* Iota Binary format version (e.g., 1) */
    uint32_t code_offset;   /* How many bytes into the file the actual code starts */
    uint32_t code_size;     /* How big the program is in bytes */
    uint32_t entry_point;   /* The memory address where the CPU should start reading */
};

#endif