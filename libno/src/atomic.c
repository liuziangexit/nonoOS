#include <atomic.h>
#include <fence.h>
#include <sync.h>

void atomic_store(uint32_t *dst, uint32_t val) {
  memory_barrier(RELEASE);
  asm("movl %0, (%1)" : : "r"(val), "r"(dst));
}

uint32_t atomic_load(uint32_t *src) {
  uint32_t val;
  asm("movl (%1), %0" : "=r"(val) : "r"(src));
  memory_barrier(ACQUIRE);
  return val;
}

uint32_t atomic_exchange(uint32_t *dst, uint32_t val) {
  uint32_t prev;
  memory_barrier(RELEASE);
  asm("xchg %1, %0" : "=m"(*dst), "=a"(prev) : "a"(val));
  memory_barrier(ACQUIRE);
  return prev;
}

bool atomic_compare_exchange(uint32_t *dst, uint32_t *expected,
                             uint32_t desired) {
  uint32_t prev = *expected;
  memory_barrier(RELEASE);
  asm("lock; cmpxchg %2, %0"
      : "=m"(*dst), "=a"(*expected)
      : "r"(desired), "a"(*expected)
      : "cc");
  memory_barrier(ACQUIRE);
  return prev == *expected;
}

uint32_t atomic_fetch_add(uint32_t *dst, uint32_t add) {
  memory_barrier(RELEASE);
  asm("lock; add %1, %0" : "=m"(*dst) : "r"(add), "m"(*dst));
  memory_barrier(ACQUIRE);
  return *dst;
}

uint32_t atomic_fetch_sub(uint32_t *dst, uint32_t sub) {
  memory_barrier(RELEASE);
  asm("lock; sub %1, %0" : "=m"(*dst) : "r"(sub), "m"(*dst));
  memory_barrier(ACQUIRE);
  return *dst;
}
