/* * IotaOS - test.s (Userland App)
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
.long  11           /* code_size: 11 bytes */
.long  0            /* entry_point: 0 (start immediately after header) */

/* --- ACTUAL PROGRAM CODE --- */
_start:
    /* Direct VGA access to prove we ran! 
       Prints a Yellow '!' at the top left of the screen */
    movl $0xB8000, %edi
    movw $0x0E21, (%edi) 
    
    /* Return back to the kernel shell */
    ret