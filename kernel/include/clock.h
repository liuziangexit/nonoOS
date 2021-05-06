#ifndef __KERNEL_CLOCK_H__
#define __KERNEL_CLOCK_H__
#include <stdlib.h>

#define TICK_PER_SECOND 100

void clock_init();
uint64_t clock_count_tick();

#endif
