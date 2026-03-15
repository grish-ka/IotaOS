/*
* IotaOS - timer.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Configures the hardware timer to tick at the given frequency (Hz) */
void timer_init(uint32_t frequency);

/* The function that runs every time the timer ticks */
uint32_t timer_handler(uint32_t esp); /* <--- Return type changed to uint32_t */

/* Get current tick count */
uint32_t timer_get_ticks(void);

#endif /* TIMER_H */