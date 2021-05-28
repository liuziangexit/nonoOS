#ifndef __LIBNO_SHARED_MEMORY_H__
#define __LIBNO_SHARED_MEMORY_H__
#define USER_SHM_ACTION_CREATE 1
#define USER_SHM_ACTION_MAP 2
#define USER_SHM_ACTION_UNMAP 3
#ifdef LIBNO_USER
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
uint32_t shared_memory_create(size_t size);
void *shared_memory_map(uint32_t id, void *addr);
void shared_memory_unmap(void *addr);
#endif
#endif
