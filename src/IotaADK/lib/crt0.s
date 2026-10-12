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