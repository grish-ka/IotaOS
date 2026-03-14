/*
* IotaOS - crt0.s
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

.code32
.section .entry, "a"  # "a" means Allocatable
.extern bin_end

/* --- IOTA BINARY (.ib) HEADER --- */
/* This data is at Byte 0 */
.short 0x4249       /* magic 'IB' */
.short 1            /* version */
.long  16           /* offset to code (this takes us to _start) */
.long  bin_end - .  /* total size */
.long  0            /* entry point */

/* Create a variable to hold the original stack pointer */
.section .bss
.global __iota_initial_esp
__iota_initial_esp: .skip 4

.section .text      /* <--- This is the 'Code' section */
.global _start
.global _iota_force_exit
.extern main

_start:
    /* Save the exact stack pointer so we can safely return to the kernel later! */
    mov %esp, __iota_initial_esp
    
    call main

    /* In C, the return value of a function is stored in EAX.
       We move it into EBX so it becomes our exit code! */
    mov %eax, %ebx

_iota_force_exit:
    /* 1. Restore the stack pointer to EXACTLY how it was when the app started */
    mov __iota_initial_esp, %esp
    
    /* 2. Call the SYS_EXIT syscall to tell the kernel we are done */
    /* (The exit code is already in EBX from our C function) */
    mov $6, %eax
    int $0x80
    
    /* 3. Because the stack is restored, 'ret' safely goes straight to ib_loader in the kernel! */
    ret