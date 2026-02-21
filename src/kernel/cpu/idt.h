#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* A single entry in the IDT (Gate Descriptor). 
   This must be exactly 64 bits (8 bytes) long! */
struct idt_entry {
    uint16_t isr_low;      /* Lower 16 bits of your C function's memory address */
    uint16_t kernel_cs;    /* The Code Segment selector (Where the kernel lives, usually 0x08) */
    uint8_t  reserved;     /* Must always be zero */
    uint8_t  attributes;   /* Flags telling the CPU what privilege level this requires */
    uint16_t isr_high;     /* Higher 16 bits of your C function's memory address */
} __attribute__((packed)); /* <--- CRITICAL! */

/* The pointer structure we hand to the CPU so it can find our table */
struct idt_ptr {
    uint16_t limit;        /* The size of the entire table in bytes, minus 1 */
    uint32_t base;         /* The memory address of the first element in our table */
} __attribute__((packed)); /* <--- CRITICAL! */

/* Function to actually set an entry in the phonebook */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

/* Function to load the IDT into the CPU */
void idt_install(void);

#endif