#ifndef __LIBNO_SYSCALL_H__
#define __LIBNO_SYSCALL_H__

#ifdef LIBNO_USER
int syscall(int num, ...);
#endif

#endif