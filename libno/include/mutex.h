#ifndef __LIBNO_MUTEX_H__
#define __LIBNO_MUTEX_H__
#include <stdint.h>

uint32_t mtx_create();
void mtx_lock();
void mtx_timedlock();
void mtx_trylock();
void mtx_unlock();

#endif