/*
 * IotaOS - rtc.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#include "drivers/rtc.h"
#include "drivers/io.h"
#include "drivers/timer.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71
#define TIMER_HZ  100

static uint32_t boot_epoch = 0;
static uint32_t boot_tick = 0;

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static uint32_t bcd(uint8_t v, int is_binary) {
    return is_binary ? v : (uint32_t)((v & 0x0F) + (v >> 4) * 10);
}

static uint32_t days_before_year(uint32_t year) {
    uint32_t days = 0;
    for (uint32_t y = 1970; y < year; y++) {
        days += ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 366 : 365;
    }
    return days;
}

void rtc_init(void) {
    /* Wait out an update in progress so the fields are consistent */
    while (cmos_read(0x0A) & 0x80) {}

    uint8_t status_b = cmos_read(0x0B);
    int binary = status_b & 0x04;
    uint32_t sec = bcd(cmos_read(0x00), binary);
    uint32_t min = bcd(cmos_read(0x02), binary);
    uint8_t raw_hour = cmos_read(0x04);
    uint32_t hour = bcd(raw_hour & 0x7F, binary);
    uint32_t day = bcd(cmos_read(0x07), binary);
    uint32_t month = bcd(cmos_read(0x08), binary);
    uint32_t year = bcd(cmos_read(0x09), binary) + 2000;

    if (!(status_b & 0x02) && (raw_hour & 0x80)) hour = (hour % 12) + 12; /* 12-hour PM */

    static const uint16_t month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint32_t days = days_before_year(year) + month_days[(month - 1) % 12] + (day - 1);
    if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) days++;

    boot_epoch = days * 86400 + hour * 3600 + min * 60 + sec;
    boot_tick = timer_get_ticks();
}

uint32_t rtc_now(void) {
    return boot_epoch + (timer_get_ticks() - boot_tick) / TIMER_HZ;
}
