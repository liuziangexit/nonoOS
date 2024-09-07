#ifndef __LIBNO_STDATOMIC_H__
#define __LIBNO_STDATOMIC_H__
#include <stdbool.h>
#include <stdint.h>

enum memory_order_t {
  memory_order_relaxed,
  // memory_order_consume,
  memory_order_acquire,
  memory_order_release,
  memory_order_acq_rel,
  memory_order_seq_cst
};
typedef enum memory_order_t memory_order;

void atomic_init(volatile uint32_t *obj, uint32_t desired);

uint32_t atomic_load(const volatile uint32_t *obj);
uint32_t atomic_load_explicit(const volatile uint32_t *obj, memory_order order);

void atomic_store(volatile uint32_t *obj, uint32_t desired);
void atomic_store_explicit(volatile uint32_t *obj, uint32_t desired,
                           memory_order order);

uint32_t atomic_exchange(volatile uint32_t *obj, uint32_t desired);
uint32_t atomic_exchange_explicit(volatile uint32_t *obj, uint32_t desired,
                                  memory_order order);

bool atomic_compare_exchange(volatile uint32_t *obj, uint32_t *expected,
                             uint32_t desired, memory_order succ,
                             memory_order fail);

// 语义等同于++i
uint32_t atomic_fetch_add(volatile uint32_t *obj, uint32_t arg);
uint32_t atomic_fetch_add_explicit(volatile uint32_t *obj, uint32_t arg,
                                   memory_order order);
uint32_t atomic_fetch_sub(volatile uint32_t *obj, uint32_t arg);
uint32_t atomic_fetch_sub_explicit(volatile uint32_t *obj, uint32_t arg,
                                   memory_order order);

#endif
