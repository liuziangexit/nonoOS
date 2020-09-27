#include "pow2_util.h"
#include <assert.h>
#include <compiler_helper.h>
#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>
#include <x86.h>

// TODO 加锁

//#define NDEBUG 1

struct page {
  struct list_entry li;
};

struct zone {
  struct page *pages;
#ifndef NDEBUG
  uint32_t cnt;
  uint32_t exp;
#endif
};

// 2^0到2^30
//这里必须是static，因为下面的代码假定这些内存已经被memset为0了
static struct zone zone[31];

static void add_page(struct zone *z, struct page *p) {
  list_init((struct list_entry *)p);
  if (z->pages) {
#ifndef NDEBUG
    // dbg模式下看看有没有重复的
    struct page *it = z->pages;
    do {
      if (it == p)
        abort();
      it = (struct page *)((struct list_entry *)it)->next;
    } while (z->pages != it);
#endif
    list_add((struct list_entry *)z->pages, (struct list_entry *)p);
  } else {
    z->pages = p;
  }
#ifndef NDEBUG
  z->cnt++;
#endif
}

static void del_page(struct zone *z, struct page *p) {
  if (z->pages) {
#ifndef NDEBUG
    // dbg模式下看看是不是真的在里面
    bool found = false;
    struct page *it = z->pages;
    do {
      if (it == p) {
        found = true;
        break;
      }
      it = (struct page *)((struct list_entry *)it)->next;
    } while (z->pages != it);
    assert(found);
#endif
    if (list_empty((struct list_entry *)z->pages)) {
      assert(z->pages == p);
      z->pages = 0;
    } else {
      if (z->pages == p) {
        struct page *next = (struct page *)list_next((struct list_entry *)p);
        list_del((struct list_entry *)p);
        z->pages = next;
      } else {
        list_del((struct list_entry *)p);
      }
    }
  } else {
    abort();
  }
#ifndef NDEBUG
  z->cnt--;
#endif
}

static uint32_t list_size(list_entry_t *head) {
  if (head == 0)
    return 0;
  uint32_t i = 0;
  bool end = false;
  do {
    i++;
    end = head->next == head;
    if (!end) {
      head = list_next(head);
      if (head == 0)
        abort();
    }
  } while (!end);
  return i;
}

//FIXME 好了，现在这个地方应该重构一下，由于对齐之类的脏活在kmem_init，也就是搞页目录的时候已经做过了
//所以这里的逻辑就变得很简单：搞个滑动窗口扫描页目录，对于每一个最大的连续内存，来按buddy的方式拆掉
//TODO kmem_init要像这里一样打印一些信息出来
void kmem_page_init(struct e820map_t *memlayout) {
  //如果要改这里，看看要不要把kmem_init.c里面相似的部分一块改了
  for (uint32_t i = 0; i < memlayout->count; i++) {
    // BIOS保留的内存
    if (!E820_ADDR_AVAILABLE(memlayout->ard[i].type)) {
#ifndef NDEBUG
      printf("kmem_page_init: address at e820[%d] is reserved, "
             "ignore\n",
             i);
#endif
      continue;
    }
    //小于1个大页的内存
    if (memlayout->ard[i].size < _4M) {
#ifndef NDEBUG
      terminal_fgcolor(CGA_COLOR_LIGHT_BROWN);
      printf("kmem_page_init: address at e820[%d] is smaller than 4M, "
             "ignore\n",
             i);
      terminal_default_color();
#endif
      continue;
    }
    //高于4G的内存，只可远观不可亵玩焉
    if (memlayout->ard[i].addr > 0xfffffffe) {
#ifndef NDEBUG
      terminal_fgcolor(CGA_COLOR_LIGHT_BROWN);
      printf("kmem_page_init: address at e820[%d] too high, ignore\n", i);
      terminal_default_color();
#endif
      continue;
    }

    // 4k对齐
    char *addr = ROUNDUP((char *)(uintptr_t)memlayout->ard[i].addr, _4K);
    uint32_t round_diff = (uintptr_t)addr - (uintptr_t)memlayout->ard[i].addr;
    int32_t page_count = (int32_t)((memlayout->ard[i].size - round_diff) / _4K);
    //如果addr小于FREESPACE（其实就是物理地址12MB起的部分），就让他等于12MB
    if ((uintptr_t)addr < KERNEL_FREESPACE) {
      if ((uint32_t)addr + (uint32_t)(page_count * _4K) >
          (uint32_t)KERNEL_FREESPACE) {
        page_count -= ((KERNEL_FREESPACE - (uintptr_t)addr) / _4K);
        addr = (char *)KERNEL_FREESPACE;
#ifndef NDEBUG
        terminal_fgcolor(CGA_COLOR_LIGHT_BROWN);
        printf("kmem_page_init: address at e820[%d] is starting at "
               "FREESPACE(12MB) now\n",
               i);
        terminal_default_color();
#endif
      } else {
#ifndef NDEBUG
        terminal_fgcolor(CGA_COLOR_LIGHT_BROWN);
        printf("kmem_page_init: address at e820[%d] is too low, "
               "ignore\n",
               i);
        terminal_default_color();
#endif
        continue;
      }
    }

#ifndef NDEBUG
    printf(
        "kmem_page_init: address at e820[%d] (%d pages) are been split into ",
        i, page_count);
#endif
    while (page_count > 0) {
      assert((uint32_t)addr % _4K == 0);
      int32_t c;
      if (page_count == 1) {
        c = 1;
      } else {
        c = prev_pow2(page_count);
      }

      //对于addr+c*4096越过2G+12M界的处理
      if ((uint32_t)addr + (uint32_t)(c * _4K) >= 0x80C00000) {
#ifndef NDEBUG
        terminal_fgcolor(CGA_COLOR_LIGHT_BROWN);
        printf("\nkmem_page_init: rest part of address %08llx at e820[%d] are "
               "too high, ignore",
               (int64_t)(uintptr_t)addr, i);
        terminal_default_color();
#endif
        break;
      }

#ifndef NDEBUG
      printf("%d ", c);
      extern uint32_t kernel_page_directory[];
      if (!pd_ismapped(kernel_page_directory, (uintptr_t)addr)) {
        terminal_fgcolor(CGA_COLOR_RED);
        printf("\n\nbad addr 0x%08llx!\n\n", (int64_t)(uintptr_t)addr);
        terminal_default_color();
      }
      volatile int *test = (volatile int *)addr;
      *test = 9710;
      assert(*test == 9710);
#endif
      struct page *pg = (struct page *)addr;
      list_init((list_entry_t *)pg);
      const uint32_t log2_c = naive_log2(c);
      if (!(zone[log2_c].pages)) {
        zone[log2_c].pages = pg;
      } else {
        list_add((list_entry_t *)(zone[log2_c].pages), (list_entry_t *)pg);
      }
#ifndef NDEBUG
      zone[log2_c].cnt++;
#endif
      page_count -= c;
      addr += (_4K * c);
#ifndef NDEBUG
      assert(((uint64_t)(uintptr_t)(pg) + (uint64_t)pow2(log2_c) * _4K) <
             0x80C00000);
#endif
    }
#ifndef NDEBUG
    printf("\n");
#endif
  }
#ifndef NDEBUG
  terminal_color(CGA_COLOR_LIGHT_GREEN, CGA_COLOR_DARK_GREY);
  printf("kmem_page_init: OK\n");
  terminal_default_color();
#endif
}

//从zone[exp]获得一个指针
//如果zone[exp]没有，则去上层要
static void *split(uint32_t exp) {
  assert(exp < sizeof(zone) / sizeof(struct zone));
#ifndef NDEBUG
  assert(list_size((list_entry_t *)zone[exp].pages) == zone[exp].cnt);
#endif

  if (zone[exp].pages) {
    //本层有，返回本层的
    struct page *rv = zone[exp].pages;
    del_page(&zone[exp], zone[exp].pages);
    return rv;
  } else {
    //本层没有
    if (exp == sizeof(zone) / sizeof(struct zone) - 1) {
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
      add_page(&zone[exp], a);
      //一半return掉
      return b;
    }
  }
}

//如果single可以在zone[exp]里找到他的partner，那就把他们俩组合起来丢到更高层的zone去
//如果single没有在zone[exp]里找到partner，那就把single丢在zone[exp]里
static void combine(uint32_t exp, void *single) {
  assert(exp < sizeof(zone) / sizeof(struct zone));
#ifndef NDEBUG
  assert(list_size((list_entry_t *)zone[exp].pages) == zone[exp].cnt);
#endif

  struct page *psingle = (struct page *)single;
  // partner可能是single+2^exp或single-2^exp
  // 看看能不能找到partner
  if (exp < sizeof(zone) / sizeof(struct zone) - 1 && zone[exp].pages != 0) {
    list_entry_t *p = (list_entry_t *)zone[exp].pages;
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
        del_page(&zone[exp], (struct page *)p);
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
      end = p->next == (list_entry_t *)zone[exp].pages;
      if (!end)
        p = list_next(p);
    } while (!end);
  }
  //没找到，把single放进链表
  add_page(&zone[exp], psingle);
}

void *kmem_page_alloc(size_t cnt) {
  cnt = next_pow2(cnt);
  return split(naive_log2(cnt));
}

void kmem_page_free(void *p, size_t cnt) {
  cnt = next_pow2(cnt);
  combine(naive_log2(cnt), p);
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
  assert(write_dump(&used, dst, dst_len, sizeof(zone) / sizeof(struct zone)));
  for (uint32_t i = 0; i < sizeof(zone) / sizeof(struct zone); i++) {
    assert(write_dump(&used, dst, dst_len, zone[i].cnt));
    if (zone[i].cnt) {
      uint32_t biggest = 0;
      for (uint32_t j = 0; j < zone[i].cnt; j++) {
        assert(write_dump(
            &used, dst, dst_len, //
            pick_biggest(&biggest, (uint32_t *)zone[i].pages, zone[i].cnt)));
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
