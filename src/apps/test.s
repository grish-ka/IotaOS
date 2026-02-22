/* * IotaOS - syscallTest.s
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

.code32
.section .text
.global _start

/* --- IOTA BINARY (.ib) HEADER --- */
.short 0x4249       /* magic: 0x4249 ('I' 'B') */
.short 1            /* version: 1 */
.long  16           /* code_offset: 16 bytes (size of this header) */
.long  44           /* code_size: 13 bytes */
.long  0            /* entry_point: 0 (start immediately after header) */

_start:
    call next_line       /* Push current EIP to stack */
next_line:
    pop %ebx             /* EBX now knows exactly where we are in RAM */
    add $(msg - next_line), %ebx /* Offset EBX to point to the string */

    mov $1, %eax         /* Syscall 1: PRINT */
    int $0x80

    ret                  /* Back to Kernel Shell */

msg:
    .asciz "IotaOS: Syscall Success!\n"