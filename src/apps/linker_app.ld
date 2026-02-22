/* * IotaOS - linker_app.ld
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

ENTRY(_start)

SECTIONS
{
    /* Anchor to Byte 0 */
    . = 0x0;

    /* The Header must be FIRST and PROTECTED */
    .entry : 
    {
        KEEP(*( .entry ))
    }

    /* The Code starts immediately after (at Byte 16) */
    .text : 
    {
        *(.text*)
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data*)   }
    .bss    : { *(.bss*)    }
    
    bin_end = .;
}