#ifndef __LIBNO_SYSCALL_H__
#define __LIBNO_SYSCALL_H__

// void SYSCALL_EXIT(err)
#define SYSCALL_EXIT 0

// ptr(分配的内存的虚拟地址，若失败为0) SYSCALL_ALLOC(alignment, length)
#define SYSCALL_ALLOC 1

// void SYSCALL_FREE(ptr)
#define SYSCALL_FREE 2

// uint32_t SYSCALL_GETPID()
#define SYSCALL_GETPID 3

// void SYSCALL_PRINTF(fmt, va_list*)
#define SYSCALL_PRINTF 4

// void SYSCALL_SCANF(fmt, va_list*)
#define SYSCALL_SCANF 5

#ifdef LIBNO_USER
int32_t syscall(int num, ...);
#endif

#endif