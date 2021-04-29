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
#include <string.h>
#include <tty.h>
#include <x86.h>

// buddy
// TODO 加锁

//#define NDEBUG 1

struct page {
  struct list_entry li;
};

struct zone {
  struct page pages;
#ifndef NDEBUG
  uint32_t cnt;
#endif
};

// 2^0到2^30
//这里必须是static，因为下面的代码假定这些内存已经被memset为0了
static struct zone zones[31];

static void add_page(struct zone *z, struct page *p) {
  list_init((struct list_entry *)p);
#ifndef NDEBUG
  // dbg模式下看看有没有重复的
  for (struct page *it = (struct page *)z->pages.li.next; //
       it != &z->pages;                                   //
       it = (struct page *)it->li.next) {
    if (it == p)
      panic("add_page check failed 1");
  }
  z->cnt++;
#endif
  list_add(&z->pages.li, (struct list_entry *)p);
}

static void del_page(struct zone *z, struct page *p) {
#ifndef NDEBUG
  // dbg模式下看看是不是真的在里面
  bool found = false;
  for (struct page *it = (struct page *)z->pages.li.next; //
       it != &z->pages;                                   //
       it = (struct page *)it->li.next) {
    if (it == p) {
      found = true;
      break;
    }
  }
  if (!found) {
    panic("del_page check failed 2");
  }
  z->cnt--;
#endif
  list_del((struct list_entry *)p);
}

//别乱用。。
static uint32_t list_size(list_entry_t *head) {
  if (head == 0)
    return 0;
  uint32_t cnt = 0;
  const list_entry_t *cp = head;
  for (list_entry_t *it = cp->next; it != cp; it = it->next) {
    cnt++;
  }
  return cnt;
}

void kmem_page_debug() {
  printf("kmem_page_debug\n");
  printf("******************************************\n");
  for (int i = 0; i < sizeof(zones) / sizeof(struct zone); i++) {
    if (zones[i].cnt)
      printf("zone for 2^%d contains %d\n", i, zones[i].cnt);
  }
  printf("******************************************\n");
}

void kmem_page_init() {
  extern uint32_t kernel_pd[1024];
  for (uint32_t i = 0; i < sizeof(zones) / sizeof(struct zone); i++) {
    list_init(&zones[i].pages.li);
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
    list_init((list_entry_t *)pg);
    const uint32_t log2_c = log2(c);
    list_add(&zones[log2_c].pages.li, (list_entry_t *)pg);

#ifndef NDEBUG
    zones[log2_c].cnt++;
#endif
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

//从zone[exp]获得一个指针
//如果zone[exp]没有，则去上层要
static void *split(uint32_t exp) {
  assert(exp < sizeof(zones) / sizeof(struct zone));
#ifndef NDEBUG
  assert(list_size(&zones[exp].pages.li) == zones[exp].cnt);
#endif

  if (!list_empty(&zones[exp].pages.li)) {
    //本层有，返回本层的
    struct page *rv = (struct page *)list_next(&zones[exp].pages.li);
    del_page(&zones[exp], rv);
    return rv;
  } else {
    //本层没有
    if (exp == sizeof(zones) / sizeof(struct zone) - 1) {
      //本层已经是最顶层，真的没有了，失败
      return 0;
    } else {
      //本层不是最顶层，向上头要
      void *a = split(exp + 1);
      if (a == 0)
        return 0;
      //把拿到的内存切两半
      void *b = a + pow2(exp) * _4K;
      //一半存本层里
      add_page(&zones[exp], a);
      //一半return掉
      return b;
    }
  }
}

//如果single可以在zone[exp]里找到他的partner，那就把他们俩组合起来丢到更高层的zone去
//如果single没有在zone[exp]里找到partner，那就把single丢在zone[exp]里
static void combine(uint32_t exp, void *single) {
  assert(exp < sizeof(zones) / sizeof(struct zone));
#ifndef NDEBUG
  assert(list_size(&zones[exp].pages.li) == zones[exp].cnt);
#endif

  struct page *psingle = (struct page *)single;
  // partner可能是single+2^exp或single-2^exp
  // 看看能不能找到partner
  if (exp < sizeof(zones) / sizeof(struct zone) - 1 &&
      !list_empty(&zones[exp].pages.li)) {
    list_entry_t *p = (list_entry_t *)zones[exp].pages.li.next;
    bool end = false;
    do {
      int found; // 0=不是，1=single是前面的，2=single是后面的
      if ((uintptr_t)single + pow2(exp) * _4K == (uintptr_t)p) {
        found = 1;
      } else if ((uintptr_t)single - pow2(exp) * _4K == (uintptr_t)p) {
        found = 2;
      } else {
        found = 0;
      }
      if (found != 0) {
        //找到了partner
        //首先把p移出链表
        del_page(&zones[exp], (struct page *)p);
        //然后向上合并
        if (found == 1) {
          // single在前面
          combine(exp + 1, single);
        } else {
          // partner在前面
          combine(exp + 1, p);
        }
        return;
      }
      end = p->next == &zones[exp].pages.li;
      if (!end)
        p = list_next(p);
    } while (!end);
  }
  //没找到，把single放进链表
  add_page(&zones[exp], psingle);
}

void *kmem_page_alloc(size_t cnt) {
  cnt = next_pow2(cnt);
  return split(log2(cnt));
}

void kmem_page_free(void *p, size_t cnt) {
  cnt = next_pow2(cnt);
  combine(log2(cnt), p);
}

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
#ifndef NDEBUG
  uint32_t used = 0;
  assert(write_dump(&used, dst, dst_len, sizeof(zones) / sizeof(struct zone)));
  for (uint32_t i = 0; i < sizeof(zones) / sizeof(struct zone); i++) {
    assert(write_dump(&used, dst, dst_len, zones[i].cnt));
    if (zones[i].cnt) {
      uint32_t biggest = 0;
      for (uint32_t j = 0; j < zones[i].cnt; j++) {
        assert(write_dump(
            &used, dst, dst_len, //
            pick_biggest(&biggest, (uint32_t *)&zones[i].pages, zones[i].cnt)));
      }
    }
  }
#else
  abort();
  __builtin_unreachable();
#endif
}

bool kmem_page_compare_dump(void *a, void *b) {
#ifndef NDEBUG
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
#else
  abort();
  __builtin_unreachable();
#endif
}

void kmem_page_print_dump(void *dmp) {
#ifndef NDEBUG
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
#else
  abort();
  __builtin_unreachable();
#endif
}
