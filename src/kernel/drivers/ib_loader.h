/* * IotaOS - ib_loader.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef IB_LOADER_H
#define IB_LOADER_H

#include <stdint.h>

#define IB_MAGIC 0x4249 /* 'I' and 'B' */

struct ib_header {
    uint16_t magic;         /* Must be 0x4249 */
    uint16_t version;       /* Iota Binary format version */
    uint32_t code_offset;   /* Bytes from start of file to actual code */
    uint32_t code_size;     /* Size of the code */
    uint32_t entry_point;   /* Usually 0 for now */
} __attribute__((packed));

void ib_load_and_run(void* file_data);

#endif