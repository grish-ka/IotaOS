/*
* IotaOS - timer.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "timer.h"
#include "io.h"
#include "stdio.h"
#include "terminal.h"

/* Keep track of how many times the system has ticked since boot */
uint32_t tick = 0;

void timer_init(uint32_t frequency) {
    /* The hardware clock ticks at exactly 1193180 Hz. 
       We divide this by our desired frequency (e.g., 100Hz). */
    uint32_t divisor = 1193180 / frequency;

    /* Send the command byte (0x36) to the PIT Command Port (0x43).
       0x36 means: Channel 0, lobyte/hibyte, Mode 3 (Square Wave), Binary */
    outb(0x43, 0x36);

    /* Split the 16-bit divisor into two 8-bit bytes */
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    /* Send the divisor to Data Port (0x40) */
    outb(0x40, low);
    outb(0x40, high);
}

/* This is called by your main IRQ handler in isr.c */
void timer_handler(void) {
    tick++;
    
    /* * In the future, this is EXACTLY where we will call our 
     * Task Scheduler to switch running processes! 
     */
}