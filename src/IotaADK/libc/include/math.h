/*
* IotaOS - math.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _MATH_H
#define _MATH_H

#define HUGE_VAL (__builtin_huge_val())

static inline double ldexp(double x, int exp) {
    while (exp > 0) { x *= 2.0; exp--; }
    while (exp < 0) { x /= 2.0; exp++; }
    return x;
}

static inline double pow(double base, double exp) {
    double res = 1.0;
    int e = (int)exp;
    if (e >= 0) {
        while (e > 0) { res *= base; e--; }
    } else {
        while (e < 0) { res /= base; e++; }
    }
    return res;
}

#endif