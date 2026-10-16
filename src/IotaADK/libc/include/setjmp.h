/*
* IotaOS - setjmp.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef _SETJMP_H
#define _SETJMP_H

typedef int jmp_buf[6];
typedef int sigjmp_buf[6];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val) __attribute__((noreturn));

int sigsetjmp(sigjmp_buf env, int savemask);
void siglongjmp(sigjmp_buf env, int val) __attribute__((noreturn));

#define sigsetjmp(env, savemask) setjmp(env)
#define siglongjmp(env, val)     longjmp(env, val)

#endif /* _SETJMP_H */