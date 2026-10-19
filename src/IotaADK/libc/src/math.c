/*
* IotaOS - math.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*
* The C math library on the x87 FPU: transcendental functions use its built-in
* instructions (fsqrt, fsin, fcos, fpatan, fyl2x, f2xm1, fscale, fprem) and
* work in 80-bit extended precision before rounding to double.
*/

#include <math.h>
#include <stdint.h>
#include <errno.h>

typedef long double ld;

static ld exp2_l(ld x);
static ld log2_l(ld x);

/* ---- bit access ---- */

static uint64_t bits(double x) {
    union { double d; uint64_t u; } v = { x };
    return v.u;
}

static double from_bits(uint64_t u) {
    union { uint64_t u; double d; } v = { u };
    return v.d;
}

/* ---- rounding (x87 control word rounding modes) ---- */

#define RC_NEAREST 0x0000
#define RC_DOWN    0x0400
#define RC_UP      0x0800
#define RC_ZERO    0x0C00

static ld round_mode(ld x, unsigned short mode) {
    unsigned short old, cw;
    __asm__ volatile("fnstcw %0" : "=m"(old));
    cw = (old & ~0x0C00) | mode;
    __asm__ volatile("fldcw %0" : : "m"(cw));
    __asm__ volatile("frndint" : "+t"(x));
    __asm__ volatile("fldcw %0" : : "m"(old));
    return x;
}

double floor(double x) { return round_mode(x, RC_DOWN); }
double ceil(double x) { return round_mode(x, RC_UP); }
double trunc(double x) { return round_mode(x, RC_ZERO); }
double rint(double x) { return round_mode(x, RC_NEAREST); }
double nearbyint(double x) { return round_mode(x, RC_NEAREST); }
ld floorl(ld x) { return round_mode(x, RC_DOWN); }
ld ceill(ld x) { return round_mode(x, RC_UP); }
ld truncl(ld x) { return round_mode(x, RC_ZERO); }
float floorf(float x) { return round_mode(x, RC_DOWN); }
float ceilf(float x) { return round_mode(x, RC_UP); }
float truncf(float x) { return round_mode(x, RC_ZERO); }

/* Halfway cases away from zero */
double round(double x) {
    if (!isfinite(x)) return x;
    ld t = round_mode(x, RC_ZERO);
    ld diff = (ld)x - t;
    if (diff >= 0.5L) t += 1;
    else if (diff <= -0.5L) t -= 1;
    return copysign((double)t, x);
}
float roundf(float x) { return round(x); }
ld roundl(ld x) { return round((double)x); }
long lround(double x) { return (long)round(x); }
long long llround(double x) { return (long long)round(x); }
long lrint(double x) { return (long)rint(x); }
long long llrint(double x) { return (long long)rint(x); }

double fabs(double x) { return from_bits(bits(x) & ~(1ULL << 63)); }
float fabsf(float x) { return fabs(x); }
ld fabsl(ld x) { return x < 0 ? -x : (x == 0 ? 0.0L : x); }

double copysign(double x, double y) {
    return from_bits((bits(x) & ~(1ULL << 63)) | (bits(y) & (1ULL << 63)));
}
ld copysignl(ld x, ld y) { return signbit(y) ? -fabsl(x) : fabsl(x); }

/* ---- square root ---- */

static ld sqrt_l(ld x) {
    __asm__("fsqrt" : "+t"(x));
    return x;
}

double sqrt(double x) {
    if (x < 0) {
        errno = EDOM;
        return NAN;
    }
    return sqrt_l(x);
}
float sqrtf(float x) { return sqrt(x); }
ld sqrtl(ld x) { return x < 0 ? NAN : sqrt_l(x); }

double cbrt(double x) {
    if (x == 0 || !isfinite(x)) return x;
    ld a = fabs(x);
    ld r = exp2_l(log2_l(a) / 3);
    /* One Newton step in extended precision tightens the result */
    r = r - (r * r * r - a) / (3 * r * r);
    return copysign((double)r, x);
}

double hypot(double x, double y) {
    if (isinf(x) || isinf(y)) return INFINITY;
    return sqrt_l((ld)x * x + (ld)y * y);
}

/* ---- remainders ---- */

static ld fprem_l(ld x, ld y, int ieee) {
    unsigned short sw;
    do {
        if (ieee) __asm__("fprem1; fnstsw %%ax" : "+t"(x), "=a"(sw) : "u"(y));
        else __asm__("fprem; fnstsw %%ax" : "+t"(x), "=a"(sw) : "u"(y));
    } while (sw & 0x0400); /* C2 set: reduction incomplete */
    return x;
}

double fmod(double x, double y) {
    if (y == 0 || isinf(x) || isnan(x) || isnan(y)) return NAN;
    if (isinf(y)) return x;
    return fprem_l(x, y, 0);
}
double remainder(double x, double y) {
    if (y == 0 || isinf(x) || isnan(x) || isnan(y)) return NAN;
    if (isinf(y)) return x;
    return fprem_l(x, y, 1);
}
float fmodf(float x, float y) { return fmod(x, y); }
ld fmodl(ld x, ld y) { return (y == 0) ? NAN : fprem_l(x, y, 0); }

double modf(double x, double *iptr) {
    double i = trunc(x);
    *iptr = i;
    if (isinf(x)) return copysign(0.0, x);
    return copysign(x - i, x);
}
ld modfl(ld x, ld *iptr) {
    ld i = truncl(x);
    *iptr = i;
    return x - i;
}

/* ---- exponent manipulation ---- */

double frexp(double x, int *exp) {
    if (x == 0 || !isfinite(x)) {
        *exp = 0;
        return x;
    }
    uint64_t u = bits(x);
    int e = (int)((u >> 52) & 0x7ff);
    if (e == 0) { /* subnormal: normalise first */
        x *= 0x1p64;
        u = bits(x);
        e = (int)((u >> 52) & 0x7ff) - 64;
    }
    *exp = e - 1022;
    return from_bits((u & ~(0x7ffULL << 52)) | (1022ULL << 52));
}

ld frexpl(ld x, int *exp) { return frexp((double)x, exp); }

static ld scale_l(ld x, ld n) {
    __asm__("fscale" : "+t"(x) : "u"(n));
    return x;
}

double ldexp(double x, int exp) { return scale_l(x, exp); }
double scalbn(double x, int exp) { return scale_l(x, exp); }
ld ldexpl(ld x, int exp) { return scale_l(x, exp); }

int ilogb(double x) {
    int e;
    if (x == 0) return -2147483647 - 1;
    if (!isfinite(x)) return 2147483647;
    frexp(x, &e);
    return e - 1;
}

double logb(double x) {
    if (x == 0) return -INFINITY;
    if (!isfinite(x)) return fabs(x);
    return ilogb(x);
}

double nextafter(double x, double y) {
    if (isnan(x) || isnan(y)) return x + y;
    if (x == y) return y;
    if (x == 0) return copysign(from_bits(1), y);
    uint64_t u = bits(x);
    if ((x < y) == (x > 0)) u++;
    else u--;
    return from_bits(u);
}

double fmax(double x, double y) { return isnan(x) ? y : isnan(y) ? x : (x > y ? x : y); }
double fmin(double x, double y) { return isnan(x) ? y : isnan(y) ? x : (x < y ? x : y); }
double fdim(double x, double y) { return x > y ? x - y : 0; }
double fma(double x, double y, double z) { return (ld)x * y + z; }

/* ---- exponentials and logarithms ---- */

/* 2^x for any finite x: split into integer and fraction, f2xm1 does the fraction */
static ld exp2_l(ld x) {
    if (isinf(x)) return x > 0 ? x : 0;
    ld n = round_mode(x, RC_NEAREST);
    ld f = x - n;
    __asm__("f2xm1" : "+t"(f));
    return scale_l(f + 1, n);
}

static ld log2_l(ld x) {
    ld one = 1;
    __asm__("fyl2x" : "+t"(x) : "u"(one) : "st(1)");
    return x;
}

double exp(double x) { return exp2_l((ld)x * 1.4426950408889634073599246810019L); }
double exp2(double x) { return exp2_l(x); }
ld expl(ld x) { return exp2_l(x * 1.4426950408889634073599246810019L); }
float expf(float x) { return exp(x); }

double expm1(double x) {
    if (fabs(x) < 1e-5) return x + x * x / 2 + x * x * x / 6;
    return (double)(exp2_l((ld)x * 1.4426950408889634073599246810019L) - 1);
}

double log(double x) {
    if (x < 0 || isnan(x)) { if (x < 0) errno = EDOM; return NAN; }
    if (x == 0) { errno = ERANGE; return -INFINITY; }
    return log2_l(x) * 0.69314718055994530941723212145818L;
}
double log2(double x) {
    if (x < 0 || isnan(x)) { if (x < 0) errno = EDOM; return NAN; }
    if (x == 0) { errno = ERANGE; return -INFINITY; }
    return log2_l(x);
}
double log10(double x) {
    if (x < 0 || isnan(x)) { if (x < 0) errno = EDOM; return NAN; }
    if (x == 0) { errno = ERANGE; return -INFINITY; }
    return log2_l(x) * 0.30102999566398119521373889472449L;
}
ld logl(ld x) { return x <= 0 ? (x == 0 ? -INFINITY : NAN) : log2_l(x) * 0.69314718055994530941723212145818L; }
float logf(float x) { return log(x); }

double log1p(double x) {
    if (fabs(x) < 1e-4) return x - x * x / 2 + x * x * x / 3;
    return log(1 + x);
}

static int is_odd_integer(double y) {
    if (fabs(y) >= 0x1p53 || trunc(y) != y) return 0;
    return fmod(y, 2.0) != 0;
}

double pow(double x, double y) {
    if (y == 0) return 1;
    if (x == 1) return 1;
    if (isnan(x) || isnan(y)) return NAN;
    if (isinf(y)) {
        if (x == -1) return 1;
        return ((fabs(x) > 1) == (y > 0)) ? INFINITY : 0;
    }
    if (x == 0) {
        if (y < 0) {
            errno = ERANGE;
            return is_odd_integer(y) ? copysign(INFINITY, x) : INFINITY;
        }
        return is_odd_integer(y) ? x : 0;
    }
    if (isinf(x)) {
        if (x > 0) return y > 0 ? INFINITY : 0;
        if (y > 0) return is_odd_integer(y) ? -INFINITY : INFINITY;
        return is_odd_integer(y) ? -0.0 : 0;
    }

    int negate = 0;
    if (x < 0) {
        if (trunc(y) != y) {
            errno = EDOM;
            return NAN;
        }
        negate = is_odd_integer(y);
        x = -x;
    }
    /* Small integer powers are exact by repeated squaring */
    if (trunc(y) == y && fabs(y) <= 64) {
        long n = (long)fabs(y);
        ld r = 1, b = x;
        while (n) {
            if (n & 1) r *= b;
            b *= b;
            n >>= 1;
        }
        if (y < 0) r = 1 / r;
        return negate ? -(double)r : (double)r;
    }
    ld r = exp2_l((ld)y * log2_l(x));
    return negate ? -(double)r : (double)r;
}
float powf(float x, float y) { return pow(x, y); }
ld powl(ld x, ld y) { return pow((double)x, (double)y); }

/* ---- trigonometry ---- */

static ld sin_l(ld x) {
    __asm__("fsin" : "+t"(x));
    return x;
}

static ld cos_l(ld x) {
    __asm__("fcos" : "+t"(x));
    return x;
}

/*
 * Reduce x to r in [-pi/4, pi/4] with x = r + q*pi/2, returning q mod 4.
 * fsin/fcos only know pi to 66 bits, so near multiples of pi their own
 * reduction is inexact; Cody-Waite with pi/2 split into 32+32+64 bits keeps
 * every product exact for |x| < 2^31. Larger arguments use the FPU's fprem1.
 */
#define PIO2_1 0xc90fdaa2p-31L
#define PIO2_2 0x85a308d3p-65L
#define PIO2_3 0x98cc51701b839a25p-132L

static ld reduce_pio2(ld x, int *quadrant) {
    if (fabsl(x) <= 0.78539816339744830961566084581988L) {
        *quadrant = 0;
        return x;
    }
    if (fabsl(x) < 0x1p31L) {
        ld k = round_mode(x * 0.63661977236758134307553505349005745L, RC_NEAREST);
        *quadrant = (int)((long long)k & 3);
        return ((x - k * PIO2_1) - k * PIO2_2) - k * PIO2_3;
    }
    /* fprem1 reports the low quotient bits in C0 (Q2), C3 (Q1) and C1 (Q0) */
    ld y = PIO2_1 + PIO2_2;
    unsigned short sw;
    do {
        __asm__("fprem1; fnstsw %%ax" : "+t"(x), "=a"(sw) : "u"(y));
    } while (sw & 0x0400);
    *quadrant = ((sw >> 8) & 1) << 2 | ((sw >> 14) & 1) << 1 | ((sw >> 9) & 1);
    *quadrant &= 3;
    return x;
}

double sin(double x) {
    if (!isfinite(x)) return NAN;
    int q;
    ld r = reduce_pio2(x, &q);
    switch (q) {
    case 0: return sin_l(r);
    case 1: return cos_l(r);
    case 2: return -sin_l(r);
    default: return -cos_l(r);
    }
}

double cos(double x) {
    if (!isfinite(x)) return NAN;
    int q;
    ld r = reduce_pio2(x, &q);
    switch (q) {
    case 0: return cos_l(r);
    case 1: return -sin_l(r);
    case 2: return -cos_l(r);
    default: return sin_l(r);
    }
}

double tan(double x) {
    if (!isfinite(x)) return NAN;
    int q;
    ld r = reduce_pio2(x, &q), one;
    __asm__("fptan" : "=t"(one), "=u"(r) : "0"(r));
    return (q & 1) ? -1 / r : r;
}
float sinf(float x) { return sin(x); }
float cosf(float x) { return cos(x); }
float tanf(float x) { return tan(x); }
float atan2f(float y, float x) { return atan2(y, x); }
float atanf(float x) { return atan(x); }
float asinf(float x) { return asin(x); }
float acosf(float x) { return acos(x); }

static ld atan2_l(ld y, ld x) {
    __asm__("fpatan" : "=t"(y) : "0"(x), "u"(y) : "st(1)");
    return y;
}

double atan2(double y, double x) { return atan2_l(y, x); }
double atan(double x) { return atan2_l(x, 1); }
double asin(double x) {
    if (fabs(x) > 1) { errno = EDOM; return NAN; }
    return atan2_l(x, sqrt_l(1 - (ld)x * x));
}
double acos(double x) {
    if (fabs(x) > 1) { errno = EDOM; return NAN; }
    return atan2_l(sqrt_l(1 - (ld)x * x), x);
}

/* ---- hyperbolic ---- */

double sinh(double x) {
    if (fabs(x) < 1e-5) return x;
    ld e = expl(x);
    return (e - 1 / e) / 2;
}
double cosh(double x) {
    ld e = expl(x);
    return (e + 1 / e) / 2;
}
double tanh(double x) {
    if (fabs(x) > 22) return copysign(1.0, x);
    if (fabs(x) < 1e-5) return x;
    ld e = expl(2 * (ld)x);
    return (e - 1) / (e + 1);
}
double asinh(double x) {
    if (fabs(x) < 1e-5) return x - (ld)x * x * x / 6;
    ld a = fabs(x);
    return copysign((double)logl(a + sqrt_l(a * a + 1)), x);
}
double acosh(double x) {
    if (x < 1) { errno = EDOM; return NAN; }
    return logl(x + sqrt_l((ld)x * x - 1));
}
double atanh(double x) {
    if (fabs(x) > 1) { errno = EDOM; return NAN; }
    if (fabs(x) == 1) { errno = ERANGE; return copysign(INFINITY, x); }
    if (fabs(x) < 1e-5) return x + (ld)x * x * x / 3;
    return 0.5L * logl((1 + (ld)x) / (1 - (ld)x));
}
