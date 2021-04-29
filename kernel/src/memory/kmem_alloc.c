#include "bare_hashmap.h"
#include "pow2_util.h"
#include <memory_manager.h>
#include <mmu.h>
#include <panic.h>

// TODO 其实这里可以key是内存地址，value是内存长度，这样就可以只用一个hashmap了

// cache_hashmap用来追踪kmem_cache_alloc分配出去的内存是属于哪一个slab里面的
// key: 分配出去的内存指针  value: slab指针
void *cache_hashmap;
uint32_t cache_hashmap_pgcnt;

// page_hashmap用来确认一个内存是不是由kmem_page_alloc分配出去的
// key: 分配出去的内存指针  value: 页数
void *page_hashmap;
uint32_t page_hashmap_pgcnt;

void kmem_alloc_init() {
  cache_hashmap = kmem_page_alloc(2);
  if (!cache_hashmap)
    panic("kmem_alloc_init allocate space for hashmap failed");
  cache_hashmap_pgcnt = 2;
  bare_init(cache_hashmap, cache_hashmap_pgcnt);

  page_hashmap = kmem_page_alloc(2);
  if (!page_hashmap)
    panic("kmem_alloc_init allocate space for hashmap failed");
  page_hashmap_pgcnt = 2;
  bare_init(page_hashmap, page_hashmap_pgcnt);
}

void *kmem_alloc(size_t alignment, size_t size) {
  if (alignment != 0) {
    assert(size % alignment == 0);
    assert(is_pow2(alignment) && alignment <= MAX_ALIGNMENT);
  }
  if (size >= 4096) {
    //页分配
    uint32_t page_cnt = ROUNDUP(size, 4096) / 4096;
    void *mem = kmem_page_alloc(page_cnt);
    if (!mem) {
      return 0;
    }
    uint32_t prev = bare_put(page_hashmap, page_hashmap_pgcnt, (uint32_t)mem,
                             page_cnt, &page_hashmap, &page_hashmap_pgcnt);
    assert(prev == 0);
    extern uint32_t kernel_pd[];
    assert(pd_lookup(kernel_pd, mem) == V2P(mem));
    return mem;
  } else {
    //对象分配
    //记录hashmap的工作由cache那边做
    void *mem = kmem_cache_alloc(alignment, size);
    extern uint32_t kernel_pd[];
    assert(pd_lookup(kernel_pd, mem) == V2P(mem));
    return mem;
  }
}

bool kmem_free(void *p) {
  assert(p);
  if (kmem_cache_free(p)) {
    return true;
  }
  if (((uintptr_t)p) % 4096 == 0) {
    uint32_t page_cnt = bare_del(page_hashmap, page_hashmap_pgcnt, (uint32_t)p);
    if (page_cnt != 0) {
      kmem_page_free(p, page_cnt);
      return true;
    }
  }
  return false;
}