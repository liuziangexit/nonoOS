#include "../src/memory/bare_hashmap.h"
#include <assert.h>
#include <defs.h>
#include <memory_manager.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>

#ifdef NDEBUG
#define __NDEBUG_BEEN_FUCKED
#undef NDEBUG
#endif

void bare_hashmap_test() {
  printf("running bare_hashmap_test\n");

  void *hashmap = kmem_page_alloc(2);
  uint32_t hashmap_pgcnt = 2;

  assert(hashmap);
  void *c_npg;
  uint32_t c_npgcnt;
  uint32_t prev =
      bare_put(hashmap, hashmap_pgcnt, 9710, 1234, &c_npg, &c_npgcnt);
  assert(prev == 0);
  assert(bare_get(hashmap, hashmap_pgcnt, 9710) == 1234);
  assert(bare_del(hashmap, hashmap_pgcnt, 9710) == 1234);
  assert(bare_get(hashmap, hashmap_pgcnt, 9710) == 0);
  prev = bare_put(hashmap, hashmap_pgcnt, 9710, 1234, &c_npg, &c_npgcnt);
  assert(prev == 0);
  prev = bare_put(hashmap, hashmap_pgcnt, 9710, 999, &c_npg, &c_npgcnt);
  assert(prev == 1234);
  prev = bare_put(hashmap, hashmap_pgcnt, 9710, 777, &c_npg, &c_npgcnt);
  assert(prev == 999);
  bare_clear(hashmap, hashmap_pgcnt);
  assert(bare_get(hashmap, hashmap_pgcnt, 9710) == 0);
  for (uint32_t i = 0; i < 1024; i++) {
    assert(bare_put(hashmap, hashmap_pgcnt, i, i + 1, &c_npg, &c_npgcnt) == 0);
  }
  for (uint32_t i = 0; i < 1024; i++) {
    assert(bare_get(hashmap, hashmap_pgcnt, i) == i + 1);
  }
  bare_clear(hashmap, hashmap_pgcnt);
  //上面都没有grow过，接下来要grow了
  for (uint32_t i = 0; i < 4096; i++) {
    assert(bare_put(hashmap, hashmap_pgcnt, i, i + 1, &hashmap,
                    &hashmap_pgcnt) == 0);
  }
  for (uint32_t i = 0; i < 4096; i++) {
    assert(bare_get(hashmap, hashmap_pgcnt, i) == i + 1);
  }

  terminal_color(CGA_COLOR_GREEN, CGA_COLOR_GREEN);
  printf(" ");
  terminal_default_color();
  printf("bare_hashmap_test passed!!!\n");
  terminal_default_color();
}

#ifdef __NDEBUG_BEEN_FUCKED
#define NDEBUG
#undef __NDEBUG_BEEN_FUCKED
#endif
