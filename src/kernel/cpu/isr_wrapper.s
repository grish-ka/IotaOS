.section .text
.extern fault_handler

/* Macro for exceptions that DO NOT push an error code (Pushes a dummy 0) */
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    cli
    pushl $0        /* Dummy error code */
    pushl $\num     /* Interrupt number */
    jmp isr_common_stub
.endm

/* Macro for exceptions that DO push an error code */
.macro ISR_ERRCODE num
.global isr\num
isr\num:
    cli
    /* The CPU already pushed the error code! */
    pushl $\num     /* Interrupt number */
    jmp isr_common_stub
.endm

/* Automatically generate all 32 exception wrappers! */
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8    /* Double Fault has an error code! */
ISR_NOERRCODE 9
ISR_ERRCODE   10   /* Invalid TSS has an error code! */
ISR_ERRCODE   11   /* Segment Not Present has an error code! */
ISR_ERRCODE   12   /* Stack Fault has an error code! */
ISR_ERRCODE   13   /* General Protection Fault has an error code! */
ISR_ERRCODE   14   /* Page Fault has an error code! */
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

/* The master tunnel that all 32 wrappers jump to */
isr_common_stub:
    pushal             /* 1. Save all general-purpose registers */

    /* ADD THIS: Push a dummy zero to keep C registers_t aligned! */
    pushl $0

    pushl %esp         /* 4. Pass the current stack pointer to C */
    call fault_handler /* 5. Jump to your C code! */
    addl $4, %esp      /* 6. Clean up the pushed argument */

    /* ADD THIS: Clean up the dummy zero we pushed! */
    addl $4, %esp      

    popal              /* 8. Restore all general-purpose registers */
    addl $8, %esp      /* 9. Clean up the error code and ISR number */
    iret               /* 10. Return! */

/* --- HARDWARE INTERRUPTS (IRQs) --- */

.global irq1
.extern irq_handler

/* IRQ 1 is the Keyboard! It maps to IDT entry 33 */
irq1:
    cli
    pushl $0        /* Dummy error code to keep the stack aligned */
    pushl $33       /* IDT Entry 33 */
    jmp irq_common_stub

/* The master tunnel for all hardware interrupts */
irq_common_stub:
    pushal

    /* ADD THIS: Push a dummy zero to keep C registers_t aligned! */
    pushl $0

    pushl %esp
    call irq_handler  /* <--- Calls our new C function! */
    addl $4, %esp     /* Clean up the pushed %esp */

    /* ADD THIS: Clean up the dummy zero we pushed! */
    addl $4, %esp

    popal
    addl $8, %esp
    iret