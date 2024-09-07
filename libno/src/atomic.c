#include <assert.h>
#include <compiler_helper.h>
#include <stdatomic.h>

void atomic_thread_fence(memory_order order) {
  if (order == memory_order_seq_cst)
    asm volatile("mfence" ::: "memory");
}

void atomic_signal_fence(memory_order order) {
  UNUSED(order);
  asm volatile("" ::: "memory");
}

void make_sure_aligned(const volatile uint32_t *ptr) {
  assert((uintptr_t)ptr % 4 == 0);
}

void atomic_init(volatile uint32_t *obj, uint32_t desired) { *obj = desired; }

uint32_t atomic_load(const volatile uint32_t *obj) {
  return atomic_load_explicit(obj, memory_order_seq_cst);
}
uint32_t atomic_load_explicit(const volatile uint32_t *obj,
                              memory_order order) {
  UNUSED(order);
  make_sure_aligned(obj);
  uint32_t val;
  asm volatile("movl %1, %0" : "=r"(val) : "m"(*obj) : "memory");
  return val;
}

void atomic_store(volatile uint32_t *obj, uint32_t desired) {
  atomic_store_explicit(obj, desired, memory_order_seq_cst);
}
void atomic_store_explicit(volatile uint32_t *obj, uint32_t desired,
                           memory_order order) {
  make_sure_aligned(obj);
  if (order == memory_order_seq_cst)
    asm volatile("mfence" : : : "memory");
  asm volatile("movl %1, %0" : "=m"(*obj) : "r"(desired) : "memory");
}

uint32_t atomic_exchange(volatile uint32_t *obj, uint32_t desired) {
  return atomic_exchange_explicit(obj, desired, memory_order_seq_cst);
}
uint32_t atomic_exchange_explicit(volatile uint32_t *obj, uint32_t desired,
                                  memory_order order) {
  UNUSED(order);
  make_sure_aligned(obj);
  uint32_t prev = desired;

  // "The XCHG instruction always asserts the LOCK# signal regardless of the
  // presence or absence of the LOCK prefix."

  // 根据
  // https://stackoverflow.com/questions/40409297/does-lock-xchg-have-the-same-behavior-as-mfence
  // 的说法，mfence和lock从ISA上看是一样的。
  // 但是因为Intel实现的疏忽，只有mfence可以阻止之后的non-temporal
  // load被重排到之前， 而lock则不行 对于我们来说，不需要考虑non-temporal
  // load，所以直接用lock就够了
  asm volatile("xchg %0, %1" : "+a"(prev), "=m"(*obj) : : "cc", "memory");
  return prev;
}

bool atomic_compare_exchange(volatile uint32_t *obj, uint32_t *expected,
                             uint32_t desired, memory_order succ,
                             memory_order fail) {
  UNUSED(succ);
  UNUSED(fail);
  make_sure_aligned(obj);
  make_sure_aligned(expected);
  uint32_t prev = *expected;
  asm volatile("lock; cmpxchg %2, %0"
               : "=m"(*obj), "+a"(*expected)
               : "r"(desired)
               : "cc", "memory");
  return prev == *expected;
}

uint32_t atomic_fetch_add(volatile uint32_t *obj, uint32_t arg) {
  return atomic_fetch_add_explicit(obj, arg, memory_order_seq_cst);
}
uint32_t atomic_fetch_add_explicit(volatile uint32_t *obj, uint32_t arg,
                                   memory_order order) {
  UNUSED(order);
  make_sure_aligned(obj);
  asm volatile("lock; xaddl %0, %1" : "+r"(arg), "+m"(*obj) : : "cc", "memory");
  return arg;
}

uint32_t atomic_fetch_sub(volatile uint32_t *obj, uint32_t arg) {
  return atomic_fetch_sub_explicit(obj, arg, memory_order_seq_cst);
}
uint32_t atomic_fetch_sub_explicit(volatile uint32_t *obj, uint32_t arg,
                                   memory_order order) {
  return atomic_fetch_add_explicit(obj, -arg, order);
}
