#include <assert.h>
#include <memory_manager.h>
#include <stdio.h>
#include <tty.h>

#ifdef NDEBUG
#define __NDEBUG_BEEN_FUCKED
#undef NDEBUG
#endif

// FIXME 现在还没有正式的全面测试，应该给cache写一个类似于page那样的测试

void kmem_cache_test() {
  printf("running kmem_cache_test\n");

  void *mem = kmem_cache_alloc(9710);
  assert(mem);
  void *mem2 = kmem_cache_alloc(9888);
  assert(mem2);
  kmem_cache_free(mem2);
  kmem_cache_free(mem);
  mem = kmem_cache_alloc(4 * 1024 * 1024);
  assert(mem);
  mem2 = kmem_cache_alloc(4 * 1024 * 1024);
  assert(mem2);
  kmem_cache_free(mem2);
  kmem_cache_free(mem);

  unsigned char dmp1[256], dmp2[256];
  kmem_page_dump(dmp1, 256);
  kmem_page_dump(dmp2, 256);
  assert(kmem_page_compare_dump(dmp1, dmp2));
  // printf("kmem_page initial state:\n");
  // kmem_page_print_dump(dmp1);

  void *look = kmem_page_alloc(8);
  assert(look);
  kmem_page_dump(dmp2, 256);
  assert(!kmem_page_compare_dump(dmp1, dmp2));
  // printf("\n\nkmem_page state after page alloc(1):\n");
  // kmem_page_print_dump(dmp2);
  kmem_page_free(look, 8);
  kmem_page_dump(dmp2, 256);
  assert(kmem_page_compare_dump(dmp1, dmp2));

  void *s[4];
  assert(s[0] = kmem_page_alloc(998));
  assert(s[1] = kmem_page_alloc(82));
  assert(s[2] = kmem_page_alloc(33));
  assert(s[3] = kmem_page_alloc(277));
  kmem_page_dump(dmp2, 256);
  assert(!kmem_page_compare_dump(dmp1, dmp2));
  // printf("\n\nkmem_page state after page "
  //       "alloc(998), alloc(82), alloc(33), alloc(227):\n");
  // kmem_page_print_dump(dmp2);
  kmem_page_free(s[3], 277);
  kmem_page_free(s[2], 33);
  kmem_page_free(s[1], 82);
  kmem_page_free(s[0], 998);
  kmem_page_dump(dmp2, 256);
  assert(kmem_page_compare_dump(dmp1, dmp2));

  terminal_color(CGA_COLOR_GREEN, CGA_COLOR_GREEN);
  printf(" ");
  terminal_default_color();
  printf("kmem_cache_test passed!!!\n");
  terminal_default_color();
}

#ifdef __NDEBUG_BEEN_FUCKED
#define NDEBUG
#undef __NDEBUG_BEEN_FUCKED
#endif
