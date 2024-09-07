#include <assert.h>
#include <stdatomic.h>
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
  atomic_store(&val, 878);
  assert(atomic_fetch_add(&val, 1) == 878);
  assert(val == 879);
  assert(atomic_fetch_sub(&val, 1) == 879);
  assert(val == 878);

  uint32_t expect = 0;
  val = 878;
  assert(!atomic_compare_exchange(&val, &expect, 0, memory_order_seq_cst,
                                  memory_order_seq_cst));
  assert(val == 878 && expect == 878);
  assert(atomic_compare_exchange(&val, &expect, 0, memory_order_seq_cst,
                                 memory_order_seq_cst));
  assert(val == 0 && expect == 878);

  val = 666;
  uint32_t lookk = atomic_exchange(&val, 5);
  assert(val == 5 && lookk == 666);

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