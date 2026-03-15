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
void timer_handler(void);

#endif /* TIMER_H */