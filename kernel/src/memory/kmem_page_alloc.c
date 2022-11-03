#include "pow2_util.h"
#include <assert.h>
#include <compiler_helper.h>
#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <panic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sync.h>
#include <tty.h>
#include <x86.h>

// buddy算法实现，管理页内存

//#define NDEBUG

struct page {
  struct list_entry head;
  uintptr_t addr;
};

// 每个zone表示一些相同页数的内存块的集合
struct zone {
  struct list_entry pages;
  bool distinct_struct;
#ifndef NDEBUG
  uint32_t cnt;
#endif
};

// 2^0到2^30
// 这里必须是static，因为下面的代码假定这些内存已经被memset为0了

// normal region的zones
static struct zone normal_region_zones[31];
// free space的zones
static struct zone free_space_zones[31];

static void add_page(struct zone *z, struct page *p) {
  make_sure_schd_disabled();
  assert(z->distinct_struct ? (uintptr_t)p != p->addr
                            : (uintptr_t)p == p->addr);
#ifndef NDEBUG
  // dbg模式下看看有没有重复的
  for (struct page *it = (struct page *)list_next(&z->pages); //
       &it->head != &z->pages;                                //
       it = (struct page *)list_next(&it->head)) {
    if (it->addr == p->addr)
      panic("add_page check failed 1");
  }
  z->cnt++;
#endif
  list_init(&p->head);
  list_add(&z->pages, &p->head);
}

static void del_page(struct zone *z, struct page *p) {
  make_sure_schd_disabled();
#ifndef NDEBUG
  // dbg模式下看看是不是真的在里面
  bool found = false;
  for (struct page *it = (struct page *)list_next(&z->pages); //
       &it->head != &z->pages;                                //
       it = (struct page *)list_next(&it->head)) {
    if (it->addr == p->addr) {
      found = true;
      break;
    }
  }
  if (!found) {
    panic("del_page check failed 2");
  }
  z->cnt--;
#endif
  list_del(&p->head);
  assert(z->distinct_struct ? (uintptr_t)p != p->addr
                            : (uintptr_t)p == p->addr);
  if (z->distinct_struct) {
    // 如果链表结构不在页上，那么我们需要额外删除这结构
    free(p);
  }
}

#ifndef NDEBUG
static uint32_t list_size(list_entry_t *head) {
  uint32_t cnt = 0;
  if (head) {
    for (list_entry_t *it = head->next; it != head; it = it->next) {
      cnt++;
    }
  }
  return cnt;
}
#endif

#ifndef NDEBUG
void kmem_page_debug() {
  printf("\n\nkmem_page_debug\n");
  printf("******************************************\n");
  for (uint32_t i = 0; i < sizeof(normal_region_zones) / sizeof(struct zone);
       i++) {
    if (normal_region_zones[i].cnt)
      printf("zone for 2^%d contains %d\n", i, normal_region_zones[i].cnt);
  }
  printf("******************************************\n\n");
}
#endif

void kmem_page_init() {
  // 初始化normal_region_zones
  for (uint32_t i = 0; i < sizeof(normal_region_zones) / sizeof(struct zone);
       i++) {
    list_init(&normal_region_zones[i].pages);
    normal_region_zones[i].distinct_struct = false;
  }
  // 初始化free_space_zones
  for (uint32_t i = 0; i < sizeof(normal_region_zones) / sizeof(struct zone);
       i++) {
    list_init(&free_space_zones[i].pages);
    free_space_zones[i].distinct_struct = true;
  }

  uintptr_t p = normal_region_vaddr;
  //这里的页都是小页
  uint32_t page_cnt = normal_region_size / _4K;

#ifndef NDEBUG
  printf("kmem_page_init: memory starts at 0x%08llx (total %d 4k pages) are "
         "being split into ",
         (int64_t)p, page_cnt);
#endif

  while (page_cnt > 0) {
    int32_t c;
    if (page_cnt == 1) {
      c = 1;
    } else {
      c = prev_pow2(page_cnt);
    }

#ifndef NDEBUG
    printf("%d ", c);
    volatile int *test = (volatile int *)p;
    *test = 9710;
    assert(*test == 9710);
#endif

    struct page *pg = (struct page *)p;
    pg->addr = p;
    const uint32_t log2_c = log2(c);
    add_page(&normal_region_zones[log2_c], pg);

    page_cnt -= c;
    p += (_4K * c);
  }

#ifndef NDEBUG
  printf("\n");
  terminal_color(CGA_COLOR_LIGHT_GREEN, CGA_COLOR_DARK_GREY);
  printf("kmem_page_init: OK\n");
  terminal_default_color();
#endif
}

void kmem_page_init_free_region(uintptr_t addr, uint32_t len) {
  uintptr_t p = addr;
  //这里的页都是小页
  uint32_t page_cnt = len / _4K;

#ifndef NDEBUG
  printf("kmem_page_init_free_region: memory starts at 0x%08llx (total %d 4k "
         "pages) are "
         "being split into ",
         (int64_t)p, page_cnt);
#endif

  while (page_cnt > 0) {
    int32_t c;
    if (page_cnt == 1) {
      c = 1;
    } else {
      c = prev_pow2(page_cnt);
    }

#ifndef NDEBUG
    printf("%d ", c);
    // TODO 把内存页map到内核空间，然后尝试访问
    // volatile int *test = (volatile int *)p;
    // *test = 9710;
    // assert(*test == 9710);
#endif

    struct page *page_struct = (struct page *)malloc(sizeof(struct page));
    assert(page_struct);
    page_struct->addr = p;
    const uint32_t log2_c = log2(c);
    add_page(&free_space_zones[log2_c], page_struct);

    page_cnt -= c;
    p += (_4K * c);
  }

#ifndef NDEBUG
  printf("\n");
#endif
}

// 从zone[exp]获得一个指针
// 如果zone[exp]没有，则去上层要
static void *split(uint32_t exp, struct zone *zones, const size_t zones_size) {
  make_sure_schd_disabled();
  assert(exp < zones_size / sizeof(struct zone));
#ifndef NDEBUG
  uint32_t look = list_size(&zones[exp].pages);
  assert(look == zones[exp].cnt);
#endif

  if (!list_empty(&zones[exp].pages)) {
    //本层有，返回本层的
    struct page *rv = (struct page *)list_next(&zones[exp].pages);
    void *addr = (void *)rv->addr;
    del_page(&zones[exp], rv);
    return addr;
  } else {
    //本层没有
    if (exp == zones_size / sizeof(struct zone) - 1) {
      //本层已经是最顶层，真的没有了，失败
      return 0;
    } else {
      //本层不是最顶层，向上头要
      void *a = split(exp + 1, zones, zones_size);
      if (a == 0)
        return 0;
      //把拿到的内存切两半
      void *b = a + pow2(exp) * _4K;
      //一半存本层里
      struct page *page_struct;
      if (zones[exp].distinct_struct) {
        page_struct = malloc(sizeof(struct page));
        assert(page_struct);
      } else {
        page_struct = a;
      }
      page_struct->addr = (uintptr_t)a;
      add_page(&zones[exp], page_struct);
      //一半return掉
      return b;
    }
  }
}

// 如果single可以在zone[exp]里找到他的partner，那就把他们俩组合起来丢到更高层的zone去
// 如果single没有在zone[exp]里找到partner，那就把single丢在zone[exp]里
static void combine(uint32_t exp, struct page *page_struct, struct zone *zones,
                    const size_t zones_size) {
  make_sure_schd_disabled();
  assert(exp < zones_size / sizeof(struct zone));
#ifndef NDEBUG
  assert(list_size(&zones[exp].pages) == zones[exp].cnt);
#endif

  // partner可能是single+2^exp或single-2^exp
  // 看看能不能找到partner
  if (exp < zones_size / sizeof(struct zone) - 1 &&
      !list_empty(&zones[exp].pages)) {
    struct page *p = (struct page *)list_next(&zones[exp].pages);
    do {
      int found; // 0=不是，1=single是前面的，2=single是后面的
      if (page_struct->addr + pow2(exp) * _4K == (uintptr_t)p) {
        found = 1;
      } else if (page_struct->addr - pow2(exp) * _4K == (uintptr_t)p) {
        found = 2;
      } else {
        found = 0;
      }
      if (found != 0) {
        // 找到了partner
        // 首先把p移出链表
        const uintptr_t partner_addr = p->addr;
        del_page(&zones[exp], p);
        // 然后向上合并
        // single在前面的情况下，不需要修改page_struct
        if (found == 2) {
          // partner在前面
          if (!zones[exp].distinct_struct) {
            page_struct = p;
          }
          page_struct->addr = partner_addr;
        }
        combine(exp + 1, page_struct, zones, zones_size);
        return;
      }
      if (list_next(&p->head) == &zones[exp].pages) {
        break;
      }
      p = (struct page *)list_next(&p->head);
    } while (true);
  }
  // 本层没他的partner，把他放进链表
  add_page(&zones[exp], page_struct);
}

void *kmem_page_alloc(size_t cnt) {
  return alloc_page_impl(NORMAL_REGION, cnt);
}

void kmem_page_free(void *p, size_t cnt) {
  free_page_impl(NORMAL_REGION, (uintptr_t)p, cnt);
}

static struct zone *get_region_zones(enum MEMORY_REGION r) {
  if (r == NORMAL_REGION) {
    return normal_region_zones;
  }
  if (r == FREE_REGION) {
    return free_space_zones;
  }
  abort();
  __unreachable;
}

static uint32_t get_region_zones_size(enum MEMORY_REGION r) {
  if (r == NORMAL_REGION) {
    return sizeof(normal_region_zones);
  }
  if (r == FREE_REGION) {
    return sizeof(free_space_zones);
  }
  abort();
  __unreachable;
}

void *alloc_page_impl(enum MEMORY_REGION r, size_t cnt) {
  cnt = next_pow2(cnt);
  SMART_CRITICAL_REGION
  void *p = split(log2(cnt), get_region_zones(r), get_region_zones_size(r));
  if (p) {
    if (r == NORMAL_REGION) {
      memset(p, 0, cnt * _4K);
    }
    if (r == FREE_REGION) {
      void *access = free_region_access(task_current()->group->vm,
                                        task_current()->group->vm_mutex,
                                        (uintptr_t)p, cnt * _4K);
      memset(access, 0, cnt * _4K);
      free_region_finish_access(task_current()->group->vm,
                                task_current()->group->vm_mutex, access);
    }
  }
  return p;
}

void free_page_impl(enum MEMORY_REGION r, uintptr_t p, size_t cnt) {
  SMART_CRITICAL_REGION
  struct page *page_struct;
  if (r == NORMAL_REGION) {
    page_struct = (struct page *)p;
  } else if (r == FREE_REGION) {
    page_struct = malloc(sizeof(struct page));
  } else {
    abort();
    __unreachable;
  }
  page_struct->addr = (uintptr_t)p;
  cnt = next_pow2(cnt);
  combine(log2(cnt), page_struct, get_region_zones(r),
          get_region_zones_size(r));
}

#ifndef NDEBUG

static bool write_dump(uint32_t *write_pos, void *dst, uint32_t dst_len,
                       uint32_t data) {
  if (*write_pos + sizeof(data) >= dst_len) {
    return false;
  }
  *(uint32_t *)(dst + *write_pos) = data;
  *write_pos += sizeof(data);
  return true;
}

static uint32_t pick_biggest(uint32_t *base, uint32_t *arr, uint32_t cnt) {
  uint32_t pick = 0;
  for (uint32_t i = 0; i < cnt; i++) {
    if (arr[cnt] > *base && arr[cnt] > pick)
      pick = arr[cnt];
  }
  *base = pick;
  return pick;
}

//成功的话require是0，如果失败是因为dst太小的缘故
/*
4:len

zone:
4:zone.cnt
n*4:zone.pages
*/
void kmem_page_dump(void *dst, uint32_t dst_len) {
  uint32_t used = 0;
  assert(write_dump(&used, dst, dst_len,
                    sizeof(normal_region_zones) / sizeof(struct zone)));
  for (uint32_t i = 0; i < sizeof(normal_region_zones) / sizeof(struct zone);
       i++) {
    assert(write_dump(&used, dst, dst_len, normal_region_zones[i].cnt));
    if (normal_region_zones[i].cnt) {
      uint32_t biggest = 0;
      for (uint32_t j = 0; j < normal_region_zones[i].cnt; j++) {
        assert(write_dump(
            &used, dst, dst_len, //
            pick_biggest(&biggest, (uint32_t *)&normal_region_zones[i].pages,
                         normal_region_zones[i].cnt)));
      }
    }
  }
}

bool kmem_page_compare_dump(void *a, void *b) {
  uint32_t *aa = (uint32_t *)a, *bb = (uint32_t *)b;
  if (*aa != *bb)
    return false;
  const uint32_t cnt = *aa;
  for (uint32_t i = 0; i < cnt; i++) {
    if (*++aa != *++bb)
      return false;
    const uint32_t zone_cnt = *aa;
    for (uint32_t j = 0; j < zone_cnt; j++) {
      if (*++aa != *++bb)
        return false;
    }
  }
  return true;
}

void kmem_page_print_dump(void *dmp) {
  printf("kmem_page_dump\n");
  printf("****************\n");
  uint32_t *dmmp = (uint32_t *)dmp;
  const uint32_t cnt = *dmmp;
  for (uint32_t i = 0; i < cnt; i++) {
    const uint32_t cunt = *++dmmp; //  :)
    terminal_fgcolor(CGA_COLOR_BLUE);
    printf("exponent:%d count:%d\n", (int32_t)i, (int32_t)cunt);
    for (uint32_t kk = 0; kk < cunt; kk++) {
      terminal_fgcolor(CGA_COLOR_LIGHT_CYAN);
      printf("0x%08llx\n", (int64_t)(uintptr_t) * ++dmmp);
    }
    terminal_default_color();
  }
  printf("****************\n");
}

#endif
