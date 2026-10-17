/*
* IotaOS - assert.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*
* Like glibc's, this header may be included more than once: each inclusion
* redefines assert() according to the current NDEBUG setting.
*/

#ifndef _ASSERT_H_DECLS
#define _ASSERT_H_DECLS
void __assert_fail(const char *expr, const char *file, int line, const char *func) __attribute__((noreturn));
#if !defined __cplusplus && !defined static_assert
#define static_assert _Static_assert
#endif
#endif

#undef assert
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : __assert_fail(#e, __FILE__, __LINE__, __func__))
#endif
