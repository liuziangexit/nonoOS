#include <compiler_helper.h>
#include <list.h>
#include <memory_manager.h>

struct slab {
  list_entry_t list;
  void *s_mem;
  uint32_t inuse;
  uint32_t free;
};

struct cache {
  list_entry_t slabs_free;
  list_entry_t slabs_partial;
  size_t objsize;
  size_t num;
};

// 2^5 - 2^17
static struct cache cache_cache[12];

static bool cache_add_slabs(struct cache *c, size_t cnt) {
  const size_t slab_size = c->num * c->objsize + sizeof(struct slab);
  const size_t slab_pgcnt = ROUNDUP(slab_size, 4096) / 4096 * cnt;
  const void *mem = kmem_page_alloc(slab_pgcnt);
  if (!mem) {
    return false;
  }

  for (size_t __i = 0; __i < cnt; __i++) {
  };
  return true;
}

static bool cache_free_slabs(struct cache *c, size_t cnt) {
  UNUSED(c);
  UNUSED(cnt);
  return false;
}

static void cache_init(struct cache *c, size_t objsize, size_t num) {
  list_init(&c->slabs_free);
  list_init(&c->slabs_partial);
  c->objsize = objsize;
  c->num = num;
}

void kmem_cache_init() {}
void *kmem_cache_alloc(size_t c) {
  UNUSED(c);
  return 0;
}
void kmem_cache_free(void *p) { UNUSED(p); }
