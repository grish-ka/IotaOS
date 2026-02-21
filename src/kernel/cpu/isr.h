/* 
 * IotaOS - isr.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/* This struct perfectly matches how our assembly wrapper pushes data onto the stack! */
typedef struct {
    uint32_t ds;                                     /* Data segment selector */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* Pushed by pushal */
    uint32_t int_no, err_code;                       /* Interrupt number and error code */
    uint32_t eip, cs, eflags, useresp, ss;           /* Pushed automatically by the CPU */
} registers_t;

#endif