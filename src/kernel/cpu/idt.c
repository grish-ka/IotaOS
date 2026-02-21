#include "idt.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void isr0(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].isr_low = (base & 0xFFFF);
    idt[num].isr_high = (base >> 16) & 0xFFFF;
    idt[num].kernel_cs = sel;
    idt[num].reserved = 0;
    idt[num].attributes = flags;
}

void idt_install(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;

    /* Clear the table */
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    /* 1. DYNAMICALLY grab the exact Code Segment the CPU is using right now */
    uint16_t current_cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(current_cs));

    /* 2. Plug that safe Code Segment into our IDT entry */
    idt_set_gate(0, (uint32_t)isr0, current_cs, 0x8E);

    /* 3. Load the IDT directly from C! (You can delete idt_load.s now!) */
    __asm__ volatile("lidt %0" : : "m"(idtp));
}