#include "bare_hashmap.h"
#include "pow2_util.h"
#include <compiler_helper.h>
#include <list.h>
#include <memory_manager.h>
#include <panic.h>
#include <stdbool.h>
#include <sync.h>

#define NDEBUG

// slab

// FIXME 现在还没有正式的全面测试，应该给cache写一个类似于page那样的测试

//对于8,16,32,64的对象，每个slab与对象在同一4k页上。这被视为“小对象”
//对于128,256,512,1k,2k，slab与objects不在同一页上，每个slab含有的objects正好一页。这被视为“大对象”

extern void *cache_hashmap;
extern uint32_t cache_hashmap_pgcnt;

struct cache;

// 24B，手动对齐到32B
struct slab {
  list_entry_t li; // 8B在cache里的链表头
  list_entry_t
      free_li; // 8B可用对象的一个双向链表(指针是位置的在4k页内的index)，
               //不需要担心链表顺序和物理顺序不一致带来cachemiss，因为对象区域最多就4k，连i3都有32k的L1
  void *mem;                // 4B连续的物理内存
  struct cache *cache;      // 4B包含本slab的cache
  uint16_t inuse;           // 2B已分配的对象数
  unsigned char padding[6]; // 6B让整个结构体对齐到32B
};

struct cache {
  list_entry_t slabs_free;    //完全空闲的slab
  list_entry_t slabs_partial; //至少分配了一个对象的slab
  size_t obj_size;            //对象大小
  size_t obj_num;             //一个slab包含的对象数
};

// 2^3B(8B) - 2^11B(2KB)
static struct cache cache_cache[9];
// 32MB的内存，用来储存大于32字节的structslab的，而这些slab的对象空间则是另外分配
// 32MB是这样算来的：每个slab管理1页内存也就是4k，那么假如说有4G的内存，最多可以有
// 2^32/2^12=1M个struct slab=32MB=8k页
static void *big_obj_slabs;
#define BIG_OBJ_SLABS_CNT 8192
static uint32_t big_obj_slabs_ctl[BIG_OBJ_SLABS_CNT / 32];

static struct slab *new_slab_struct() {
  for (uint32_t i = 0; i < BIG_OBJ_SLABS_CNT; i++) {
    if (big_obj_slabs_ctl[i] != 0xFFFFFFFF) {
      uint32_t idx = bit_scan_forward(bit_flip(big_obj_slabs_ctl[i]));
      big_obj_slabs_ctl[i] = bit_set(big_obj_slabs_ctl[i], idx, true);
      return ((struct slab *)big_obj_slabs) + i * 32 + idx;
    }
  }
  panic("new_slab_struct");
  __unreachable;
}

static void free_slab_struct(struct slab *s) {
  assert((uintptr_t)s > (uintptr_t)big_obj_slabs &&
         (uintptr_t)s <= (uintptr_t)big_obj_slabs +
                             (sizeof(struct slab) * BIG_OBJ_SLABS_CNT) &&
         ((uintptr_t)s - (uintptr_t)big_obj_slabs) % sizeof(struct slab) == 0);
  uint32_t idx =
      ((uintptr_t)s - (uintptr_t)big_obj_slabs) / sizeof(struct slab);
  uint32_t bit_idx = idx - ROUNDDOWN(idx, 32);
  idx = ROUNDDOWN(idx, 32);
  assert(bit_test(big_obj_slabs_ctl[idx], bit_idx));
  big_obj_slabs_ctl[idx] = bit_set(big_obj_slabs_ctl[idx], bit_idx, false);
}

//向cache中增加指定数量的slab
static bool cache_add_slabs(struct cache *c, size_t slab_cnt) {
  for (size_t i = 0; i < slab_cnt; i++) {
    struct slab *s = 0;
    // slab的内部布局
    if (log2(c->obj_size) < 7) {
      //小对象
      const size_t slab_pgcnt = slab_cnt;
      //分配这么多4k页
      const void *mem = kmem_page_alloc(slab_pgcnt);
      if (!mem) {
        return false;
      }
      s = (struct slab *)(mem + 4096 * i);
      s->mem = s + 1;
    } else {
      //大对象
      s = new_slab_struct();
      s->mem = kmem_page_alloc(1);
      if (!s->mem) {
        free_slab_struct(s);
        return false;
      }
    }
    list_init(&s->li);
    list_init(&s->free_li);
    s->inuse = 0;
    s->cache = c;

    //设置free链表
    for (size_t j = 0; j < c->obj_num; j++) {
      list_add(&s->free_li, (list_entry_t *)(s->mem + c->obj_size * j));
    }

    list_add(&c->slabs_free, &s->li);
  }
  return true;
}

static void cache_init(struct cache *c, size_t objsize, size_t num) {
  list_init(&c->slabs_free);
  list_init(&c->slabs_partial);
  c->obj_size = objsize;
  c->obj_num = num;
}

//模块初始化
void kmem_cache_init() {
#ifndef NDEBUG
  printf("sizeof(struct slab) == %d\n", sizeof(struct slab));
#endif
  assert(sizeof(struct slab) == 32);
  big_obj_slabs = kmem_page_alloc(BIG_OBJ_SLABS_CNT);
  if (!big_obj_slabs)
    panic("kmem_cache_init allocate space for big_obj_slabs failed");
  //初始化caches
  uint32_t exp = 3;
  for (size_t i = 0; i < sizeof(cache_cache) / sizeof(struct cache);
       i++, exp++) {
    uint32_t obj_per_slab, obj_size = pow2(exp);
    if (exp < 7) {
      //小对象
      obj_per_slab = (4096 - sizeof(struct slab)) / obj_size;
    } else {
      //大对象
      obj_per_slab = 4096 / obj_size;
    }
    cache_init(cache_cache + i, obj_size, obj_per_slab);
  }
#ifndef NDEBUG
  terminal_color(CGA_COLOR_LIGHT_GREEN, CGA_COLOR_DARK_GREY);
  printf("kmem_cache_init: OK\n");
  terminal_default_color();
#endif
}

// 在slab链表里找到合适的slab然后分配一个对象
static void *find_free_slab(list_entry_t *head, uint32_t alignment,
                            struct slab **slab) {
  list_entry_t *it = list_next(head);
  while (it != head) {
    struct slab *s = (struct slab *)it;
    if (s->cache->obj_num > s->inuse) {
      assert(!list_empty(&s->free_li));
      void *ret = 0;
      //寻找符合额外对齐要求的对象
      list_entry_t *it = list_next(&s->free_li);
      while (it != &s->free_li) {
        if (((uintptr_t)it) % alignment == 0) {
          ret = it;
          break;
        }
        it = list_next(it);
      }
      if (ret) {
        //找到对象了
        s->inuse++;
        list_del((list_entry_t *)ret);
        // debug检查越界
        if (log2(s->cache->obj_size) < 7) {
          assert(ret < ((void *)s) + 4096 &&
                 ret >= ((void *)s) + sizeof(struct slab));
        } else {
          assert(ret < ((void *)s->mem) + 4096 && ret >= ((void *)s->mem));
        }
        //记录分配的对象和slab的对应关系
        uint32_t prev =
            bare_put(cache_hashmap, cache_hashmap_pgcnt, (uint32_t)ret,
                     (uint32_t)s, &cache_hashmap, &cache_hashmap_pgcnt);
        assert(prev == 0);
        *slab = s;
        return ret;
      }
      //没有找到符合对齐要求的对象，继续...
    }
    it = list_next(it);
  }
  return 0;
}

// 模块接口，分配对齐到alignment，size大小的内存
// alignment参数指定要求的对齐值，此值必须在[1,32]之间，并且必须是2的幂。
// 无论alignment是多少，至少也会对齐到大于等于size的最小的2的幂（由slab的内部布局决定，在本文件中搜索"slab的内部布局"可以找到相关代码段）
void *kmem_cache_alloc(size_t alignment, size_t size) {
  //指定对齐
  assert(alignment != 0);
  assert(is_pow2(alignment) && alignment <= MAX_ALIGNMENT);
  //处理size参数，寻找合适的对象池(cache)
  if (size == 0)
    size = 1;
  size_t exp = log2(next_pow2(size));
  if (exp < 3)
    exp = 3;
  if (exp >= 3 + sizeof(cache_cache) / sizeof(struct cache))
    panic("kmem_cache_alloc failed"); //没有这么大的对象池
  //合适的对象池
  struct cache *cache = (cache_cache + (exp - 3));
  assert(cache->obj_size >= size);

  SMART_CRITICAL_REGION

  //在对象池里的“已部分使用的slab”中分配
  struct slab *alloc_from = 0;
  void *object = find_free_slab(&cache->slabs_partial, alignment, &alloc_from);
  if (!object) {
    //如果没有，在“未使用的slab”中分配
    object = find_free_slab(&cache->slabs_free, alignment, &alloc_from);
    if (!object) {
      //如果还是没有，说明1)此cache里所有的slab都正好没有指定对齐的对象，或者2)此cache里没有任何slab
      //需要增加一个slab
      if (!cache_add_slabs(cache, 1)) {
        //如果增加失败（分配不到这么多连续物理页），那么真的失败了
        return 0;
      }
      //如果增加成功，再在“未使用的slab”中寻找可用的slab
      object = find_free_slab(&cache->slabs_free, alignment, &alloc_from);
      //此次必然成功，确认一下
      assert(object);
    }
    //从“未使用的slab”链表里移除这slab，将它加入“已部分使用的slab”链表
    list_del(&alloc_from->li);
    list_add(&cache->slabs_partial, &alloc_from->li);
  }
  memset(object, 0, next_pow2(size));
  return object;
}

//将对象归还到slab中
static void slab_free(struct slab *s, void *p) {
  // debug检查越界
  if (log2(s->cache->obj_size) < 7) {
    assert(p < ((void *)s) + 4096 && p >= ((void *)s) + sizeof(struct slab));
  } else {
    assert(p < ((void *)s->mem) + 4096 && p >= ((void *)s->mem));
  }

  //修改计数
  assert(s->inuse != 0);
  s->inuse--;
  //将p加入free链表
  list_add(&s->free_li, (list_entry_t *)p);
  //如果slab是全空的，移入slabs_free链表
  if (s->inuse == 0) {
    list_del(&s->li);
    list_add(&s->cache->slabs_free, &s->li);
  }
}

//模块接口，归还内存
bool kmem_cache_free(void *p) {
  assert(p);
  SMART_CRITICAL_REGION
  //找到p所属的slab
  uint32_t s = bare_del(cache_hashmap, cache_hashmap_pgcnt, (uint32_t)p);
  if (!s) {
    return false;
  }
  //将p归还到slab中
  slab_free((struct slab *)s, p);
  return true;
}
