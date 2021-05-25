#include <assert.h>
#include <atomic.h>
#include <spinlock.h>
#ifndef NDEBUG
#include <mmu.h>
#include <sync.h>
#include <x86.h>
#endif

#ifndef LIBNO_USER

void spin_init(spinlock_t *l) { l->val = 0; }

void spin_lock(spinlock_t *l) {
#ifndef NDEBUG
  make_sure_schd_disabled();
#endif
  uint32_t expected = 0;
  while (!atomic_compare_exchange(&l->val, &expected, 1)) {
    expected = 0;
    asm volatile("pause");
  }
}

bool spin_trylock(spinlock_t *l) {
  uint32_t expected = 0;
  return atomic_compare_exchange(&l->val, &expected, 1);
}

void spin_unlock(spinlock_t *l) {
#ifdef NDEBUG
  atomic_store(&l->val, 0);
#else
  assert(1 == atomic_exchange(&l->val, 0));
#endif
}

#endif
