/* * IotaOS - test.s
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

.code32
.section .text
.global _start

_start:
    /* --- IOTA BINARY (.ib) HEADER --- */
    .short 0x4249       /* magic: 'IB' */
    .short 1            /* version: 1 */
    .long  16           /* code_offset */
    .long  (test_end - _start) 
    .long  0            /* entry_point */

/* The actual logic starts at offset 16 */
actual_start:
    call next_line
next_line:
    pop %ebx
    add $(msg - next_line), %ebx

    mov $1, %eax
    int $0x80
    ret

msg:
    .asciz "IotaOS: Syscall Success!\n"

test_end: