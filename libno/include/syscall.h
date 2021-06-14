#ifndef __LIBNO_SYSCALL_H__
#define __LIBNO_SYSCALL_H__
#include "stdint.h"

// uint32_t SYSCALL_TASK(action, ...)
#define SYSCALL_TASK 0

// ptr(分配的内存的虚拟地址，若失败为0) SYSCALL_ALLOC(alignment, length)
#define SYSCALL_ALLOC 1

// void SYSCALL_FREE(ptr)
#define SYSCALL_FREE 2

// void SYSCALL_PRINTF(fmt, va_list*)
#define SYSCALL_PRINTF 3

// void SYSCALL_SCANF(fmt, va_list*)
#define SYSCALL_SCANF 4

// void SYSCALL_SHM(action, ...)
#define SYSCALL_SHM 5

// void SYSCALL_MTX(action, ...)
#define SYSCALL_MTX 6

// void SYSCALL_CV(action, ...)
#define SYSCALL_CV 7

#ifdef LIBNO_USER
uint32_t syscall(int call, int cnt, ...);
#endif

#endif