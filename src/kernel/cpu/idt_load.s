.global idt_load
.type idt_load, @function

idt_load:
    /* The C function passed the memory address of our idt_ptr as an argument.
       In 32-bit C, the first argument is sitting on the stack at 4(%esp). */
    movl 4(%esp), %eax
    
    /* The 'lidt' instruction tells the CPU: "Here is the phonebook!" */
    lidt (%eax)
    
    /* Return back to the C code */
    ret