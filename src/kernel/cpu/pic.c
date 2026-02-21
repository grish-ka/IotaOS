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
    uint8_t a1, a2;

    /* Save current masks */
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

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

    /* --- CHANGE THIS BOTTOM PART --- */
    /* Mask all interrupts except IRQ 1 (Keyboard) */
    outb(PIC1_DATA, 0xFD); /* 1111 1101 */
    outb(PIC2_DATA, 0xFF); /* 1111 1111 */
}