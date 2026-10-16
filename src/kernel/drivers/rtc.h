/*
 * IotaOS - rtc.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef RTC_H
#define RTC_H

#include <stdint.h>

/* Read the CMOS clock once; later times are derived from the timer tick */
void rtc_init(void);

/* Seconds since 1970-01-01 00:00:00 UTC */
uint32_t rtc_now(void);

#endif
