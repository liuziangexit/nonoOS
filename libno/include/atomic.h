#ifndef __LIBNO_ATOMIC_H__
#define __LIBNO_ATOMIC_H__
#include <stdbool.h>
#include <stdint.h>

void atomic_store(uint32_t *dst, uint32_t val);
uint32_t atomic_load(uint32_t *src);
uint32_t atomic_exchange(uint32_t *dst, uint32_t val);
bool atomic_compare_exchange(uint32_t *dst, uint32_t *expected,
                             uint32_t desired);
// 等同于++i
uint32_t atomic_add(uint32_t *dst, uint32_t add);
uint32_t atomic_sub(uint32_t *dst, uint32_t sub);
// 等同于i++
uint32_t atomic_fetch_add(uint32_t *dst, uint32_t add);
uint32_t atomic_fetch_sub(uint32_t *dst, uint32_t sub);

#endif
