/*
* IotaOS - pic.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "drivers/io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01

/* Remaps the PICs so IRQs 0-15 fire on IDT entries 32-47 */
void pic_remap(void) {
    /* Save current masks */
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);
    (void)a1; (void)a2;

    /* Start the initialization sequence (in cascade mode) */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    /* ICW2: Master PIC vector offset (Map IRQ 0-7 to IDT 32-39) */
    outb(PIC1_DATA, 0x20); 
    /* ICW2: Slave PIC vector offset (Map IRQ 8-15 to IDT 40-47) */
    outb(PIC2_DATA, 0x28); 

    /* ICW3: Tell Master PIC there is a slave PIC at IRQ2 (0000 0100) */
    outb(PIC1_DATA, 4);
    /* ICW3: Tell Slave PIC its cascade identity (0000 0010) */
    outb(PIC2_DATA, 2);

    /* ICW4: Set x86 mode */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    /* Mask all interrupts except IRQ 0 (Timer), IRQ 1 (Keyboard), and IRQ 2 (Cascade) */
    outb(PIC1_DATA, 0xF8); 
    /* Mask all slave interrupts except IRQ 12 (PS/2 Mouse, bit 4) */
    outb(PIC2_DATA, 0xEF); 
}

/* Enable (masked=0) or disable (masked!=0) a single IRQ line without
   touching the other lines on the same PIC. */
void pic_irq_mask(uint8_t irq, int masked) {
    uint16_t data_port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = (uint8_t)(1 << (irq & 7));
    uint8_t mask = inb(data_port);
    if (masked) mask |= bit;
    else        mask &= (uint8_t)~bit;
    outb(data_port, mask);
}