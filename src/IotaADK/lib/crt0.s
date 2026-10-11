/* 
 * IotaOS - crt0.s
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

.code32
.section .text
.global _start
.extern main

_start:
    /* Jump directly into the C application */
    push %ebp
    mov %esp, %ebp
    and $-16, %esp
    call main
    mov %ebp, %esp
    pop %ebp

    /* Return to the kernel's ELF Loader when main() finishes */
    ret