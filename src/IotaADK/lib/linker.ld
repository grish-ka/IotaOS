/*
 * IotaOS - linker.ld
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

ENTRY(_start)

SECTIONS
{
    /* Tell the ELF loader to place this app at the 128 MB mark in RAM */
    . = 0x8000000;

    .text : ALIGN(4K)
    {
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }
}