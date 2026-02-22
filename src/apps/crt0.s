/* * IotaOS - crt0.s
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */
.code32
.section .entry, "a"  # "a" means Allocatable
.extern bin_end

// DOING: #1 [BUG] `.ib` header being ignored

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
    /* Write '!' (Yellow on Blue) to the top-left of the screen */
    /* 0xB8000 is the start of VGA text memory */
    mov $0xB8000, %edi
    movb $'!', (%edi)      /* Character */
    movb $0x1E, 1(%edi)    /* Attribute: 1=Blue BG, E=Yellow FG */

    /* This code is at Byte 16 */
    call main

    ret