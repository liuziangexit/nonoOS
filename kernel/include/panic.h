#ifndef __KERNEL_PANIC_H__
#define __KERNEL_PANIC_H__
#include <stdio.h>
#include <tty.h>

void panic(const char *message);

#endif
