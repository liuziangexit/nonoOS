#ifndef __KERNEL_SYSCALL_H__
#define __KERNEL_SYSCALL_H__
#include "interrupt.h"

void syscall_dispatch(struct trapframe *);

#endif
