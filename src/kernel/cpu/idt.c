#include "idt.h"

/* Create the actual table of 256 entries */
struct idt_entry idt[256];
struct idt_ptr idtp;

/* We need an assembly function to actually tell the CPU where the IDT is.
   We will write this in a moment! */
extern void idt_load(uint32_t idt_ptr_address);

/* This function fills out a single entry in the phonebook */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    /* The CPU splits the memory address into two pieces */
    idt[num].isr_low = (base & 0xFFFF);
    idt[num].isr_high = (base >> 16) & 0xFFFF;

    idt[num].kernel_cs = sel;
    idt[num].reserved = 0;
    
    /* We MUST set the ring level. For syscalls later, we will change this! */
    idt[num].attributes = flags;
}

/* This is the function we will call in kernel_main to start the IDT */
void idt_install(void) {
    /* Set up the pointer for the CPU */
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;

    /* Clear out the entire table with zeros to be safe */
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    /* Point the CPU's internal register to our table! */
    idt_load((uint32_t)&idtp);
}