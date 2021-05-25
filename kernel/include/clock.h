#ifndef __KERNEL_CLOCK_H__
#define __KERNEL_CLOCK_H__
#include <stdlib.h>

#define TICK_TIME_MS 10
#define TICK_PER_SECOND (1000 / TICK_TIME_MS)

void clock_init();
uint64_t clock_count_tick();
uint64_t clock_get_ticks();

#endif
