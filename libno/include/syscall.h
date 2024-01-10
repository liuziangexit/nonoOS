#ifndef __LIBNO_SYSCALL_H__
#define __LIBNO_SYSCALL_H__
#include "stdint.h"

// uint32_t SYSCALL_TASK(action, ...)
#define SYSCALL_TASK 1

// ptr(分配的内存的虚拟地址，若失败为0) SYSCALL_ALLOC(alignment, length)
#define SYSCALL_ALLOC 2

// void SYSCALL_FREE(ptr)
#define SYSCALL_FREE 3

// void SYSCALL_PRINTF(fmt, va_list*)
#define SYSCALL_PRINTF 4

// void SYSCALL_SCANF(fmt, va_list*)
#define SYSCALL_SCANF 5

// void SYSCALL_SHM(action, ...)
#define SYSCALL_SHM 6

// void SYSCALL_MTX(action, ...)
#define SYSCALL_MTX 7

// void SYSCALL_CV(action, ...)
#define SYSCALL_CV 8

// char* SYSCALL_GETS(char*)
#define SYSCALL_GETS 9

// int SYSCALL_GETCHAR()
#define SYSCALL_GETCHAR 10

// bool SYSCALL_SIGNAL_SET_HANDLER(pid_t pid, int sig, void (*handler)(int))
#define SYSCALL_SIGNAL_SET_HANDLER 11

// int SYSCALL_SIGNAL_WAIT(const sigset_t *restrict set, int *restrict sig)
#define SYSCALL_SIGNAL_WAIT 12

// bool SYSCALL_SIGNAL_FIRE(pid_t pid, bool group, int sig)
#define SYSCALL_SIGNAL_FIRE 13

#ifdef LIBNO_USER
uint32_t syscall(int call, int cnt, ...);
#endif

#endif