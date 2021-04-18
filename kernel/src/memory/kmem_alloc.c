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
  list_entry_t li; //在cache里的链表头
  void *s_mem;     //连续的物理内存
  size_t inuse;    //已分配的对象数
  //可用对象的一个单向链表，用对象的那块内存作为链表节点，每个节点其实就是一个uintptr_t，指向下一个可用的对象
  //在归还顺序与分配顺序不一致的情况下，链表的顺序可能与这些对象的物理地址的顺序有很大差别，这会导致cache
  // miss吧，
  //如果有必要的话，是不是归还的时候做个排序？但那样的话复杂度就变成O(n)了，但即使这样也比cache
  // miss快一个数量级
  uintptr_t free;

  struct cache *cache;   //包含本slab的cache
  size_t share_cnt;      //共享同一片内存的slab数量
  bool is_pghead;        //此slab是否是内存头
  list_entry_t share_li; //链接共享此内存的所有slab
};

struct cache {
  list_entry_t slabs_free;    //完全空闲的slab
  list_entry_t slabs_partial; //至少分配了一个对象的slab
  size_t objsize;             //对象大小
  size_t num;                 //一个slab包含的对象数
};

// 2^5 - 2^28
static struct cache cache_cache[23];

// hashmap用来追踪kmem_alloc分配出去的内存是属于哪一个slab里面的
// key: 分配出去的内存指针  value: slab指针
void *hashmap;
uint32_t hashmap_pgcnt;

//计算slab需要的内存
static inline size_t cal_slab_size(size_t objsize, size_t num) {
  return objsize * num + sizeof(struct slab);
}

//向cache中增加指定数量的slab
static bool cache_add_slabs(struct cache *c, size_t slab_cnt) {
  //计算一个slab多大
  const size_t slab_size = cal_slab_size(c->objsize, c->num);
  //这至少是多少个4k页呢
  const size_t slab_pgcnt = ROUNDUP(slab_size, 4096) * slab_cnt / 4096;
  //分配这么多4k页
  const void *mem = kmem_page_alloc(slab_pgcnt);
  if (!mem) {
    return false;
  }

  // TODO 下面还没加注释
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

#define DEFAULT_OBJ_PER_SLAB 32

//模块初始化
void kmem_cache_init() {
  uint32_t exp = 5;
  for (size_t i = 0; i < sizeof(cache_cache) / sizeof(struct cache);
       i++, exp++) {
    const size_t objsize = pow2(exp);
    uint32_t obj_per_slab;
    //这是临时的一个策略，我觉得还是要实现当内存不足时，回收其他类型的slab的功能
    if (objsize * DEFAULT_OBJ_PER_SLAB >= kmem_total_mem() / 4) {
      if (objsize < kmem_total_mem() / 4) {
        //如果对象大小的32倍大于内存的25%，但是单个对象没有内存的25%那么大，那么每个slab中的对象加起来不超过内存的25%
        obj_per_slab = kmem_total_mem() / 4 / objsize;
      } else {
        //如果对象大小的32倍大于内存的25%，并且单个对象超过内存的25%，那么不支持这么大的对象
        obj_per_slab = 0;
      }
    } else {
      //如果对象大小的32倍小于内存的25%，那么每个slab含有32个对象
      obj_per_slab = DEFAULT_OBJ_PER_SLAB;
    }
    cache_init(cache_cache + i, objsize, obj_per_slab);
  }
  //拿8k内存给hashmap
  hashmap = kmem_page_alloc(2);
  if (!hashmap)
    panic("kmem_cache_init allocate space for hashmap failed");
  hashmap_pgcnt = 2;
  bare_init(hashmap, hashmap_pgcnt);
}

//在slab链表里寻找至少还能分配一个对象的slab
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

//从slab中分配一个对象
static void *slab_alloc(struct slab *s) {
  //确认此slab至少还有1个对象可以分配
  assert(s->cache->num > s->inuse);
  //计数
  s->inuse++;
  //确认free链表不是空的
  assert(s->free);
  //分配的对象就是free链表的第一个元素
  void *rv = (void *)s->free;
  // debug检查越界
  assert(rv < ((void *)s) + cal_slab_size(s->cache->objsize, s->cache->num) &&
         rv >= (void *)s + sizeof(struct slab));
  //将分配的对象从链表中移除
  s->free = *(uintptr_t *)rv;
  //记录分配的对象和slab的对应关系
  uint32_t prev = bare_put(hashmap, hashmap_pgcnt, (uint32_t)rv, (uint32_t)s,
                           &hashmap, &hashmap_pgcnt);
  assert(prev == 0);
  return rv;
}

//模块接口，分配c大小的内存
void *kmem_cache_alloc(size_t c) {
  //寻找合适的对象池(cache)
  c = next_pow2(c);
  c = naive_log2(c);
  if (c < 5)
    c = 5;
  if (c > 5 + sizeof(cache_cache) / sizeof(struct cache))
    panic("kmem_cache_alloc failed"); //没有这么大的对象池
  //合适的对象池
  struct cache *cache = (cache_cache + (c - 5));
  //在对象池里的“已部分使用的slab”中寻找可用的slab
  list_entry_t *free_slab = find_free_slab(&cache->slabs_partial);
  if (!free_slab) {
    //如果没有，在“未使用的slab”中寻找可用的slab
    free_slab = find_free_slab(&cache->slabs_free);
    if (!free_slab) {
      //如果还是没有，说明1)此cache里所有的slab都分配满了，或者2)此cache里没有任何slab
      //所以，向cache里增加1个slab
      if (!cache_add_slabs(cache, 1)) {
        //如果增加失败（分配不到这么多连续物理页），那么真的失败了
        return 0;
      }
      //如果增加成功，再在“未使用的slab”中寻找可用的slab
      free_slab = find_free_slab(&cache->slabs_free);
      //此次必然成功，确认一下
      assert(free_slab);
    }
    //从“未使用的slab”链表里移除这slab，将它加入“已部分使用的slab”链表
    list_del(free_slab);
    list_add(&cache->slabs_partial, free_slab);
  }
  //从这slab中分配对象
  return slab_alloc((struct slab *)free_slab);
}

//将对象归还到slab中
static void slab_free(struct slab *s, void *p) {
  // debug检查越界
  assert(p < ((void *)s) + cal_slab_size(s->cache->objsize, s->cache->num) &&
         p >= (void *)s + sizeof(struct slab));

  // debug时候把回收的内存清空
  // TODO
  // 出于进程之间隔离的考虑，要不要总是把这片内存清空？看看其他的os是怎么做的
  // 不过就算要清空，我估计也不是在这里，因为这个slab的分配算法是在内核里用的
#ifndef NDEBUG
  memset(p, 0, s->cache->objsize);
#endif

  //修改计数
  assert(s->inuse != 0);
  s->inuse--;
  //修改free链表，将p作为链表的首个元素
  *(uintptr_t *)p = s->free;
  s->free = (uintptr_t)p;

  if (s->inuse == 0) {
    list_del(&s->li);
    list_add(&s->cache->slabs_free, &s->li);
  }
}

//模块接口，归还内存
void kmem_cache_free(void *p) {
  if (!p) {
    return;
  }
  //找到p所属的slab
  uint32_t s = bare_del(hashmap, hashmap_pgcnt, (uint32_t)p);
  if (!s) {
    //没有p对应的记录，说明肯定是调用者写错代码了
    abort(); //进程都给你扬啰
  }
  //将p归还到slab中
  slab_free((struct slab *)s, p);
}
