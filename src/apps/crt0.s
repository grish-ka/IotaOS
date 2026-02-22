/* * IotaOS - crt0.s
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */
.code32
.section .entry
.extern bin_end

/* --- IOTA BINARY (.ib) HEADER --- */
/* This data is at Byte 0 */
.short 0x4249       /* magic 'IB' */
.short 1            /* version */
.long  16           /* offset to code (this takes us to _start) */
.long  bin_end - .   /* total size */
.long  0            /* entry point */

.section .text      /* <--- This is the 'Code' section */
.global _start
.extern main

_start:
    /* This code is at Byte 16 */
    call main