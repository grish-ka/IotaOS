/*
* IotaOS - setjmp.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "setjmp.h"

__attribute__((naked)) int setjmp(jmp_buf env) {
    __asm__ volatile (
        "mov 4(%esp), %eax\n"  /* Get the jmp_buf pointer */
        "mov %ebx, 0(%eax)\n"  /* Save EBX */
        "mov %esi, 4(%eax)\n"  /* Save ESI */
        "mov %edi, 8(%eax)\n"  /* Save EDI */
        "mov %ebp, 12(%eax)\n" /* Save EBP */
        "lea 4(%esp), %ecx\n"  /* Calculate ESP before the call */
        "mov %ecx, 16(%eax)\n" /* Save ESP */
        "mov 0(%esp), %ecx\n"  /* Get the return address (EIP) */
        "mov %ecx, 20(%eax)\n" /* Save EIP */
        "xor %eax, %eax\n"     /* Return 0 */
        "ret\n"
    );
}

__attribute__((naked)) void longjmp(jmp_buf env, int val) {
    __asm__ volatile (
        "mov 4(%esp), %edx\n"  /* Get the jmp_buf pointer */
        "mov 8(%esp), %eax\n"  /* Get the return value */
        "test %eax, %eax\n"    /* If val is 0, force it to 1 */
        "jnz 1f\n"
        "inc %eax\n"
        "1:\n"
        "mov 0(%edx), %ebx\n"  /* Restore EBX */
        "mov 4(%edx), %esi\n"  /* Restore ESI */
        "mov 8(%edx), %edi\n"  /* Restore EDI */
        "mov 12(%edx), %ebp\n" /* Restore EBP */
        "mov 16(%edx), %esp\n" /* Restore ESP */
        "mov 20(%edx), %ecx\n" /* Get the return address (EIP) */
        "jmp *%ecx\n"          /* Jump to the saved EIP */
    );
}

int (sigsetjmp)(sigjmp_buf env, int savemask) __attribute__((alias("setjmp")));
void (siglongjmp)(sigjmp_buf env, int val) __attribute__((alias("longjmp")));