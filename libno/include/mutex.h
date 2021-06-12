#ifndef __LIBNO_MUTEX_H__
#define __LIBNO_MUTEX_H__

#define USER_MTX_ACTION_CREATE 1
#define USER_MTX_ACTION_LOCK 2
#define USER_MTX_ACTION_TIMEDLOCK 3
#define USER_MTX_ACTION_TRYLOCK 4
#define USER_MTX_ACTION_UNLOCK 5

#ifdef LIBNO_USER
#include <stdint.h>
uint32_t mtx_create();
void mtx_lock(uint32_t id);
void mtx_timedlock(uint32_t id);
void mtx_trylock(uint32_t id);
void mtx_unlock(uint32_t id);
#endif

#endif