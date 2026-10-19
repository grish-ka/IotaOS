/*
* IotaOS - time.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "time.h"
#include <sys/time.h>
#include <sys/times.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "iota.h"

/* There are no time zones: local time is UTC */

static const char *const day_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *const month_names[] = {"January", "February", "March", "April", "May", "June",
                                          "July", "August", "September", "October", "November", "December"};

static int is_leap(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int days_in_month(int month, int year) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return days[month] + (month == 1 && is_leap(year));
}

time_t time(time_t *tloc) {
    time_t now = iota_syscall3(IOTA_SYS_TIME, 0, 0, 0);
    if (tloc) *tloc = now;
    return now;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    (void)tz;
    if (tv) {
        uint32_t ticks = 0;
        tv->tv_sec = iota_syscall3(IOTA_SYS_TIME, 0, (uint32_t)&ticks, 0);
        tv->tv_usec = (ticks % 100) * 10000;
    }
    return 0;
}

int settimeofday(const struct timeval *tv, const struct timezone *tz) {
    (void)tv;
    (void)tz;
    return 0;
}

clock_t clock(void) {
    uint32_t ticks = 0;
    iota_syscall3(IOTA_SYS_TIME, 0, (uint32_t)&ticks, 0);
    return (clock_t)ticks * (CLOCKS_PER_SEC / 100);
}

clock_t times(struct tms *buf) {
    uint32_t ticks = 0;
    iota_syscall3(IOTA_SYS_TIME, 0, (uint32_t)&ticks, 0);
    if (buf) {
        buf->tms_utime = ticks;
        buf->tms_stime = buf->tms_cutime = buf->tms_cstime = 0;
    }
    return ticks;
}

double difftime(time_t time1, time_t time0) {
    return (double)(time1 - time0);
}

struct tm *gmtime_r(const time_t *timep, struct tm *tm) {
    long t = *timep;
    long days = t / 86400;
    long rem = t % 86400;
    if (rem < 0) {
        rem += 86400;
        days--;
    }
    tm->tm_hour = rem / 3600;
    tm->tm_min = (rem % 3600) / 60;
    tm->tm_sec = rem % 60;
    tm->tm_wday = (4 + days) % 7; /* 1970-01-01 was a Thursday */
    if (tm->tm_wday < 0) tm->tm_wday += 7;

    int year = 1970;
    while (days < 0) {
        year--;
        days += is_leap(year) ? 366 : 365;
    }
    for (;;) {
        int len = is_leap(year) ? 366 : 365;
        if (days < len) break;
        days -= len;
        year++;
    }
    tm->tm_year = year - 1900;
    tm->tm_yday = days;
    int month = 0;
    while (days >= days_in_month(month, year)) {
        days -= days_in_month(month, year);
        month++;
    }
    tm->tm_mon = month;
    tm->tm_mday = days + 1;
    tm->tm_isdst = 0;
    return tm;
}

struct tm *gmtime(const time_t *timep) {
    static struct tm tm;
    return gmtime_r(timep, &tm);
}

struct tm *localtime_r(const time_t *timep, struct tm *tm) {
    return gmtime_r(timep, tm);
}

struct tm *localtime(const time_t *timep) {
    return gmtime(timep);
}

/* Weak: bash carries its own mktime fallback */
__attribute__((weak)) time_t mktime(struct tm *tm) {
    /* Normalise month overflow, then count days from the epoch */
    int year = tm->tm_year + 1900 + tm->tm_mon / 12;
    int mon = tm->tm_mon % 12;
    if (mon < 0) {
        mon += 12;
        year--;
    }
    long days = 0;
    if (year >= 1970) {
        for (int y = 1970; y < year; y++) days += is_leap(y) ? 366 : 365;
    } else {
        for (int y = year; y < 1970; y++) days -= is_leap(y) ? 366 : 365;
    }
    for (int m = 0; m < mon; m++) days += days_in_month(m, year);
    days += tm->tm_mday - 1;
    time_t t = days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
    gmtime_r(&t, tm);
    return t;
}

void tzset(void) {}

char *asctime(const struct tm *tm) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%.3s %.3s %2d %02d:%02d:%02d %d\n",
             day_names[tm->tm_wday], month_names[tm->tm_mon], tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
    return buf;
}

char *ctime(const time_t *timep) {
    return asctime(localtime(timep));
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    size_t o = 0;
    char tmp[64];
    /* Year of century (00-99), valid also for years before 1900 */
    int yy2 = (tm->tm_year + 1900) % 100;
    if (yy2 < 0) yy2 += 100;

    for (const char *f = format; *f; f++) {
        if (*f != '%') {
            if (o + 1 >= max) return 0;
            s[o++] = *f;
            continue;
        }
        f++;
        if (*f == '-' || *f == '_' || *f == '0' || *f == 'E' || *f == 'O') f++; /* flags: ignored */

        tmp[0] = '\0';
        switch (*f) {
        case 'a': snprintf(tmp, sizeof(tmp), "%.3s", day_names[tm->tm_wday]); break;
        case 'A': snprintf(tmp, sizeof(tmp), "%s", day_names[tm->tm_wday]); break;
        case 'b':
        case 'h': snprintf(tmp, sizeof(tmp), "%.3s", month_names[tm->tm_mon]); break;
        case 'B': snprintf(tmp, sizeof(tmp), "%s", month_names[tm->tm_mon]); break;
        case 'c': snprintf(tmp, sizeof(tmp), "%.3s %.3s %2d %02d:%02d:%02d %d", day_names[tm->tm_wday],
                           month_names[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
                           tm->tm_year + 1900); break;
        case 'C': snprintf(tmp, sizeof(tmp), "%02d", (tm->tm_year + 1900) / 100); break;
        case 'd': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_mday); break;
        case 'D': snprintf(tmp, sizeof(tmp), "%02d/%02d/%02d", tm->tm_mon + 1, tm->tm_mday, yy2); break;
        case 'e': snprintf(tmp, sizeof(tmp), "%2d", tm->tm_mday); break;
        case 'F': snprintf(tmp, sizeof(tmp), "%d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday); break;
        case 'H': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_hour); break;
        case 'I': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_hour % 12 ? tm->tm_hour % 12 : 12); break;
        case 'j': snprintf(tmp, sizeof(tmp), "%03d", tm->tm_yday + 1); break;
        case 'k': snprintf(tmp, sizeof(tmp), "%2d", tm->tm_hour); break;
        case 'l': snprintf(tmp, sizeof(tmp), "%2d", tm->tm_hour % 12 ? tm->tm_hour % 12 : 12); break;
        case 'm': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_mon + 1); break;
        case 'M': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_min); break;
        case 'n': snprintf(tmp, sizeof(tmp), "\n"); break;
        case 'p': snprintf(tmp, sizeof(tmp), "%s", tm->tm_hour < 12 ? "AM" : "PM"); break;
        case 'P': snprintf(tmp, sizeof(tmp), "%s", tm->tm_hour < 12 ? "am" : "pm"); break;
        case 'r': snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d %s", tm->tm_hour % 12 ? tm->tm_hour % 12 : 12,
                           tm->tm_min, tm->tm_sec, tm->tm_hour < 12 ? "AM" : "PM"); break;
        case 'R': snprintf(tmp, sizeof(tmp), "%02d:%02d", tm->tm_hour, tm->tm_min); break;
        case 's': {
            struct tm copy = *tm;
            snprintf(tmp, sizeof(tmp), "%ld", (long)mktime(&copy));
            break;
        }
        case 'S': snprintf(tmp, sizeof(tmp), "%02d", tm->tm_sec); break;
        case 't': snprintf(tmp, sizeof(tmp), "\t"); break;
        case 'T':
        case 'X': snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec); break;
        case 'u': snprintf(tmp, sizeof(tmp), "%d", tm->tm_wday ? tm->tm_wday : 7); break;
        case 'w': snprintf(tmp, sizeof(tmp), "%d", tm->tm_wday); break;
        case 'x': snprintf(tmp, sizeof(tmp), "%02d/%02d/%02d", tm->tm_mon + 1, tm->tm_mday, yy2); break;
        case 'y': snprintf(tmp, sizeof(tmp), "%02d", yy2); break;
        case 'Y': snprintf(tmp, sizeof(tmp), "%d", tm->tm_year + 1900); break;
        case 'z': snprintf(tmp, sizeof(tmp), "+0000"); break;
        case 'Z': snprintf(tmp, sizeof(tmp), "UTC"); break;
        case '%': snprintf(tmp, sizeof(tmp), "%%"); break;
        case '\0': f--; break;
        default: snprintf(tmp, sizeof(tmp), "%%%c", *f); break;
        }
        size_t len = strlen(tmp);
        if (o + len >= max) return 0;
        memcpy(s + o, tmp, len);
        o += len;
    }
    if (o >= max) return 0;
    s[o] = '\0';
    return o;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    uint32_t ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;
    iota_sleep(ms ? ms : 1);
    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}
