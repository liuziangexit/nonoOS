#include <atomic.h>
#include <memory_barrier.h>

void atomic_store(uint32_t *dst, uint32_t val) {
  memory_barrier(RELEASE);
  asm volatile("movl %1, %0" : "=m"(*dst) : "r"(val) : "memory");
}

uint32_t atomic_load(uint32_t *src) {
  uint32_t val;
  asm volatile("movl %1, %0" : "=r"(val) : "m"(*src) : "memory");
  memory_barrier(ACQUIRE);
  return val;
}

uint32_t atomic_exchange(uint32_t *dst, uint32_t val) {
  uint32_t prev;
  memory_barrier(RELEASE);
  asm volatile("xchg %1, %0"
               : "=m"(*dst), "=a"(prev)
               : "a"(val)
               : "cc", "memory");
  memory_barrier(ACQUIRE);
  return prev;
}

bool atomic_compare_exchange(uint32_t *dst, uint32_t *expected,
                             uint32_t desired) {
  uint32_t prev = *expected;
  memory_barrier(RELEASE);
  asm volatile("lock; cmpxchg %2, %0"
               : "=m"(*dst), "=a"(*expected)
               : "r"(desired), "a"(*expected)
               : "cc", "memory");
  memory_barrier(ACQUIRE);
  return prev == *expected;
}

uint32_t atomic_add(uint32_t *dst, uint32_t add) {
  memory_barrier(RELEASE);
  asm volatile("lock; add %1, %0"
               : "=m"(*dst)
               : "r"(add), "m"(*dst)
               : "cc", "memory");
  memory_barrier(ACQUIRE);
  return *dst;
}

uint32_t atomic_sub(uint32_t *dst, uint32_t sub) {
  memory_barrier(RELEASE);
  asm volatile("lock; sub %1, %0"
               : "=m"(*dst)
               : "r"(sub), "m"(*dst)
               : "cc", "memory");
  memory_barrier(ACQUIRE);
  return *dst;
}

uint32_t atomic_fetch_add(uint32_t *dst, uint32_t add) {
  memory_barrier(RELEASE);
  asm volatile("lock; xaddl %0, %1" : "+r"(add), "+m"(*dst) : : "cc", "memory");
  memory_barrier(ACQUIRE);
  return add;
}

// 不知道为什么报错说没有xsubl这个指令，先不管
// uint32_t atomic_fetch_sub(uint32_t *dst, uint32_t sub) {
//   memory_barrier(RELEASE);
//   asm("lock; xsubl %0, %1" : "+r"(sub), "+m"(*dst) : : "cc", "memory");
//   memory_barrier(ACQUIRE);
//   return sub;
// }
