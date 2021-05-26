#include <assert.h>
#include <atomic.h>
#include <stdio.h>
#include <tty.h>

#ifdef NDEBUG
#define __NDEBUG_BEEN_FUCKED
#undef NDEBUG
#endif

void atomic_test() {
  printf("running atomic_test\n");

  uint32_t val;
  atomic_store(&val, 999);
  assert(atomic_load(&val) == 999 && 999 == val);
  assert(atomic_add(&val, 1) == 1000);
  assert(val == 1000);
  assert(atomic_exchange(&val, 888) == 1000);
  assert(atomic_sub(&val, 10) == 878);
  assert(val == 878);
  assert(atomic_fetch_add(&val, 1) == 878);
  assert(val == 879);
  // assert(atomic_fetch_sub(&val, 1) == 879);
  // assert(val == 878);

  uint32_t expect = 0;
  val = 878;
  assert(!atomic_compare_exchange(&val, &expect, 0));
  assert(val == 878 && expect == 878);
  assert(atomic_compare_exchange(&val, &expect, 0));
  assert(val == 0 && expect == 878);

  terminal_color(CGA_COLOR_GREEN, CGA_COLOR_GREEN);
  printf(" ");
  terminal_default_color();
  printf("atomic_test passed!!!\n");
  terminal_default_color();
}

#ifdef __NDEBUG_BEEN_FUCKED
#define NDEBUG
#undef __NDEBUG_BEEN_FUCKED
#endif