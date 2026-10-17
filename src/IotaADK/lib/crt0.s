/* 
 * IotaOS - crt0.s
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */
.code32
.section .text
.global _start
.extern libc_entry

_start:
    /*
     * IotaOS programs run in ring 0. If the CPL in CS is not 0, this binary
     * was started by another OS (e.g. a configure test run on a Linux build
     * machine): exit at once with status 127 using the Linux exit call.
     */
    mov %cs, %ax
    test $3, %ax
    jz 1f
    mov $1, %eax
    mov $127, %ebx
    int $0x80
1:
    /* Set up a standard stack frame and align the stack to 16 bytes for GCC */
    push %ebp
    mov %esp, %ebp
    and $-16, %esp

    /* Call the new libc entry point to parse argc/argv */
    call libc_entry

    /* Move the return value of main() into ebx as the exit code */
    mov %eax, %ebx   
    
    /* Syscall 6: SYS_EXIT (Cleans up open files and reloads the shell) */
    mov $6, %eax     
    int $0x80        

    /* Fallback infinite loop just in case SYS_EXIT fails */
    cli
1:  hlt
    jmp 1b