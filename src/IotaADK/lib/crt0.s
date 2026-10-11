/* 
 * IotaOS - crt0.s
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

.code32
.section .entry, "a"  # "a" means Allocatable
.extern bin_end

/* If the C code doesn't define it, it defaults to 0! */
.weak __iota_app_flags

/* --- IOTA BINARY (.ib) HEADER --- */
/* This data is at Byte 0 */
.short 0x4249           /* magic 'IB' */
.short 1                /* version */
.long  20               /* offset to code (this takes us to _start) */
.long  bin_end - .      /* total size */
.long  0                /* entry point */
.long  __iota_app_flags /* Will be 1 if macro is used, or 0 */

.section .text      /* <--- This is the 'Code' section */
.global _start
.extern main

_start:
    /* This code is at Byte 16 */
    call main

    ret