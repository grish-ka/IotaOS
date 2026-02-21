.section .text
.global isr0
.extern isr0_handler

isr0:
    pushal 
    call isr0_handler
    popal 
    iret