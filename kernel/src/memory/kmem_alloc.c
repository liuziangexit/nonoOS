#include "bare_hashmap.h"
#include "pow2_util.h"
#include <compiler_helper.h>
#include <list.h>
#include <memory_manager.h>
#include <panic.h>
#include <stdbool.h>

// 加注释

// FIXME
// 没有对齐到对象长度，比如说，1024的对象要对齐到1024边界。现在还没有这么做，这会导致速度变慢
// FIXME 现在还没有正式的全面测试，应该给cache写一个类似于page那样的测试

struct cache;

struct slab {
  list_entry_t li;
  void *s_mem;
  size_t inuse;
  uintptr_t free;

  struct cache *cache;
  size_t share_cnt;      //共享同一片内存的slab数量
  bool is_pghead;        //此slab是否是内存头
  list_entry_t share_li; //链接共享此内存的所有slab
};

struct cache {
  list_entry_t slabs_free;
  list_entry_t slabs_partial;
  size_t objsize;
  size_t num;
};

// 2^5 - 2^28
static struct cache cache_cache[23];

// hashmap用来追踪kmem_alloc分配出去的内存是属于哪一个slab里面的
// key: 分配出去的内存指针  value: slab指针
void *hashmap;
uint32_t hashmap_pgcnt;

static inline size_t cal_slab_size(size_t objsize, size_t num) {
  return objsize * num + sizeof(struct slab);
}

static bool cache_add_slabs(struct cache *c, size_t slab_cnt) {
  const size_t slab_size = cal_slab_size(c->objsize, c->num);
  const size_t slab_pgcnt = ROUNDUP(slab_size, 4096) * slab_cnt / 4096;
  const void *mem = kmem_page_alloc(slab_pgcnt);
  if (!mem) {
    return false;
  }
  list_entry_t *share_li = 0;

  for (size_t __i = 0; __i < slab_cnt; __i++) {
    struct slab *s = (struct slab *)(mem + ROUNDUP(slab_size, 4096) * __i);
    list_init(&s->li);
    s->inuse = 0;
    s->s_mem =
        (void *)(mem + ROUNDUP(slab_size, 4096) * __i) + sizeof(struct slab);
    s->free = (uintptr_t)s->s_mem;
    s->cache = c;
    s->share_cnt = slab_cnt;
    s->is_pghead = __i == 0;
    list_init(&s->share_li);
    if (__i == 0) {
      share_li = &s->share_li;
    } else {
      list_add(share_li, &s->li);
    }

    //设置单向链表
    for (size_t __j = 0; __j < c->num; __j++) {
      void *p = (void *)(s->s_mem + __j * c->objsize);
      if (__j != c->num - 1) {
        *(uintptr_t *)(p) = (uintptr_t)(s->s_mem + (__j + 1) * c->objsize);
      } else {
        *(uintptr_t *)(p) = 0;
      }
    }

    list_add(&c->slabs_free, &s->li);
  }
  return true;
}

/*
1.不需要destroy cache
2.在内存不足的时候，没有shrink其他cache，“损人利己”的逻辑
所以暂时这几个函数用不上，也因此没有经过测试，以后要用的时候记得写测试
static bool slab_share_checkfree(struct slab *s) {
  assert(s->is_pghead);
  list_entry_t *it = &s->share_li;
  do {
    if (((struct slab *)it)->inuse != 0) {
      return false;
    }
    it = list_next(it);
  } while (it != &s->share_li);
  return true;
}

static void slab_share_dofree(struct slab *s) {
  assert(s->is_pghead);
  size_t pgcnt = cal_slab_size(s->cache->objsize, s->cache->num);
  pgcnt = ROUNDUP(pgcnt, 4096);
  pgcnt *= s->share_cnt;

  kmem_page_free(s, pgcnt);
}

static bool cache_free_slabs(struct cache *c, size_t cnt) {
  list_entry_t *it = &c->slabs_free;
  do {
    struct slab *s = (struct slab *)it;
    if (s->share_cnt == cnt && s->is_pghead) {
      if (slab_share_checkfree(s)) {
        slab_share_dofree(s);
        return true;
      }
    }
    it = list_next(it);
  } while (it != &c->slabs_free);
  return false;
}*/

static void cache_init(struct cache *c, size_t objsize, size_t num) {
  list_init(&c->slabs_free);
  list_init(&c->slabs_partial);
  c->objsize = objsize;
  c->num = num;
}

void kmem_cache_init() {
  uint32_t exp = 5;
  for (size_t i = 0; i < sizeof(cache_cache) / sizeof(struct cache);
       i++, exp++) {
    const size_t objsize = pow2(exp);
    cache_init(cache_cache + i, objsize, 32);
  }
  hashmap = kmem_page_alloc(2);
  if (!hashmap)
    panic("kmem_cache_init allocate space for hashmap failed");
  hashmap_pgcnt = 2;
  bare_init(hashmap, hashmap_pgcnt);
}

static list_entry_t *find_free_slab(list_entry_t *head) {
  list_entry_t *it = list_next(head);
  while (it != head) {
    struct slab *s = (struct slab *)it;

    if (s->cache->num > s->inuse) {
      return it;
    }

    it = list_next(it);
  }
  return 0;
}

static void *slab_alloc(struct slab *s) {
  assert(s->cache->num > s->inuse);
  s->inuse++;
  assert(s->free);
  void *rv = (void *)s->free;
  // debug检查越界
  assert(rv < ((void *)s) + cal_slab_size(s->cache->objsize, s->cache->num) &&
         rv >= (void *)s + sizeof(struct slab));
  s->free = *(uintptr_t *)rv;
  //记录rv和s的对应关系
  uint32_t prev = bare_put(hashmap, hashmap_pgcnt, (uint32_t)rv, (uint32_t)s,
                           &hashmap, &hashmap_pgcnt);
  assert(prev == 0);
  return rv;
}

void *kmem_cache_alloc(size_t c) {
  c = next_pow2(c);
  c = naive_log2(c);
  if (c < 5)
    c = 5;
  if (c > 5 + sizeof(cache_cache) / sizeof(struct cache))
    panic("kmem_cache_alloc failed");
  struct cache *cache = (cache_cache + (c - 5));
  list_entry_t *free_slab = find_free_slab(&cache->slabs_partial);
  if (!free_slab) {
    free_slab = find_free_slab(&cache->slabs_free);
    if (!free_slab) {
      if (!cache_add_slabs(cache, 1)) {
        return 0;
      }
      free_slab = find_free_slab(&cache->slabs_free);
      assert(free_slab);
    }
    list_del(free_slab);
    list_add(&cache->slabs_partial, free_slab);
  }
  return slab_alloc((struct slab *)free_slab);
}

static void slab_free(struct slab *s, void *p) {
  // debug检查越界
  assert(p < ((void *)s) + cal_slab_size(s->cache->objsize, s->cache->num) &&
         p >= (void *)s + sizeof(struct slab));

  // debug时候把回收的内存清空
#ifndef NDEBUG
  memset(p, 0, s->cache->objsize);
#endif

  assert(s->inuse != 0);
  s->inuse--;
  *(uintptr_t *)p = s->free;
  s->free = (uintptr_t)p;

  if (s->inuse == 0) {
    list_del(&s->li);
    list_add(&s->cache->slabs_free, &s->li);
  }
}

void kmem_cache_free(void *p) {
  if (!p) {
    return;
  }
  uint32_t s = bare_del(hashmap, hashmap_pgcnt, (uint32_t)p);
  if (!s) {
    abort(); //进程都给你扬啰
  }
  slab_free((struct slab *)s, p);
}
