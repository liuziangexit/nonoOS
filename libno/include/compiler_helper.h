#ifndef __LIBNO_COMPILER_HELPER_H__
#define __LIBNO_COMPILER_HELPER_H__

//消除编译器unused的变量警告
#define UNUSED(x) (void)(x)

// GNU
#ifdef __GNUC__

#define __always_inline inline __attribute__((always_inline))
#define __noinline __attribute__((noinline))
#define __noreturn __attribute__((noreturn))

#define __unreachable __builtin_unreachable();

#endif

#endif