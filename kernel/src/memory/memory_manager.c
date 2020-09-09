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

void kmem_init() {
  extern uint32_t kernel_page_directory[];
  //如果直接修改正在使用的页目录，会翻车
  //所以lcr3一个一模一样的页目录副本，这样我们才能开始修改真正的页目录
  _Alignas(4096) uint32_t temp_pd[1024];
  memcpy(temp_pd, kernel_page_directory, 4096);
  union {
    struct CR3 cr3;
    uintptr_t val;
  } cr3;
  cr3.val = 0;
  set_cr3(&(cr3.cr3), V2P((uintptr_t)temp_pd), false, false);
  lcr3(cr3.val);
  //好吧，现在开始修改真的页目录(kernel_page_directory)
  memset(kernel_page_directory, 0, 4096);
  //虚拟地址0到4M -> 物理地址0到4M（为了CGA这样的外设地址）
  pd_map_ps(kernel_page_directory, 0, 0, 1, PTE_P | PTE_W | PTE_PS);
  // 8M内核映像
  pd_map_ps(kernel_page_directory, KERNEL_VIRTUAL_BASE, 0, 2,
            PTE_P | PTE_W | PTE_PS);
  // 4M内核栈
  pd_map_ps(kernel_page_directory, KERNEL_VIRTUAL_BASE + KERNEL_STACK,
            KERNEL_STACK, 1, PTE_P | PTE_W | PTE_PS);
  // 2G free space，这个物理上是在kernel image和kernel
  // stack之后，直到物理内存结束的这块区域，
  // 把它map到虚拟内存12MB的位置
  pd_map_ps(kernel_page_directory, KERNEL_FREESPACE, KERNEL_FREESPACE, 512,
            PTE_P | PTE_W | PTE_PS);

  //重新加载真的页目录(kernel_page_directory)
  cr3.val = 0;
  set_cr3(&(cr3.cr3), V2P((uintptr_t)kernel_page_directory), false, false);
  lcr3(cr3.val);
}

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

static inline bool is_pow2(uint32_t x) { return !(x & (x - 1)); }

//输入2的n次幂，返回n
//如果输入不是2的幂次，行为未定义
static uint32_t naive_log2(uint32_t x) {
  assert(is_pow2(x));
  uint32_t idx;
  asm("bsrl %1, %0" : "=r"(idx) : "r"(x));
  return idx;
}

//返回2^exp
static uint32_t pow2(uint32_t exp) { return 1 << exp; }

//找到第一个大于等于x的2的幂
//如果x已经是2的幂，返回x
// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static uint32_t next_pow2(uint32_t x) {
  assert(x > 0);
  if (is_pow2(x))
    return x;
  //下面这段代码啥意思呢，我们都知道2的幂的二进制表示是一个1后面跟着许多0的，
  //所以第一个大于x的2的幂应该是x的最高位左边那一位是1的一个数(比如1010的就是10000)
  //这一段代码做的是把x的位全部变成1，然后+1自然就是结果了
  //比如1010，首先把它变成1111，然后加1不就是10000了吗
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

//最小的有效输入是2
//如果x已经是2的幂，返回x
static inline uint32_t prev_pow2(uint32_t x) {
  assert(x >= 2);
  if (is_pow2(x))
    return x;
  return next_pow2(x) / 2;
}

void kmem_page_init(struct e820map_t *memlayout) {
  for (int i = 0; i < memlayout->count; i++) {
    // BIOS保留的内存
    if (!E820_ADDR_AVAILABLE(memlayout->ard[i].type)) {
#ifndef NDEBUG
      printf("kmem_page_init: address at e820[%d] is reserved, "
             "ignore\n",
             i);
#endif
      continue;
    }
    //小于1页的内存
    if (memlayout->ard[i].size < 4096) {
#ifndef NDEBUG
      terminal_fgcolor(CGA_COLOR_LIGHT_BROWN);
      printf("kmem_page_init: address at e820[%d] is smaller than a page, "
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
    char *addr = ROUNDUP((char *)(uintptr_t)memlayout->ard[i].addr, 4096);
    uint32_t round_diff = (uintptr_t)addr - (uintptr_t)memlayout->ard[i].addr;
    int32_t page_count =
        (int32_t)((memlayout->ard[i].size - round_diff) / 4096);
    //如果addr小于FREESPACE（其实就是物理地址12MB起的部分），就让他等于12MB
    if ((uintptr_t)addr < KERNEL_FREESPACE) {
      if ((int32_t)addr + page_count * 4096 > KERNEL_FREESPACE) {
        page_count -= ((KERNEL_FREESPACE - (uintptr_t)addr) / 4096);
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
      assert((uint32_t)addr % 4096 == 0);
      int32_t c;
      if (page_count == 1) {
        c = 1;
      } else {
        c = prev_pow2(page_count);
      }

      //对于addr+c*4096越过2G+12M界的处理
      if ((uint32_t)addr >= 0x80C00000) {
#ifndef NDEBUG
        terminal_fgcolor(CGA_COLOR_LIGHT_BROWN);
        printf("kmem_page_init: some part of address %08llx at e820[%d] are "
               "too high, ignore\n",
               (int64_t)(uintptr_t)addr, i);
        terminal_default_color();
#endif
        break;
      }

#ifndef NDEBUG
      printf("%d ", c);
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
      addr += (4096 * c);
#ifndef NDEBUG
      assert(((uint64_t)(uintptr_t)(zone[log2_c].pages) +
              (uint64_t)pow2(log2_c) * zone[log2_c].cnt * 4096) < 0x80C00000);
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

  if (zone[exp].pages != 0) {
    //本层有，返回本层的
    if (list_empty((list_entry_t *)zone[exp].pages)) {
      //如果本层里只有最后一个了
      struct page *rv = zone[exp].pages;
      zone[exp].pages = 0;
      return rv;
    } else {
      //如果本层里还有许多个
      struct page *rv = zone[exp].pages;
      struct page *future_head =
          (struct page *)list_next((list_entry_t *)zone[exp].pages);
      list_del((list_entry_t *)zone[exp].pages);
      zone[exp].pages = future_head;
      return rv;
    }
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
      void *b = a + pow2(exp) * 4096;
      //一半存本层里
      assert(zone[exp].pages != 0);
      list_init((list_entry_t *)a);
      zone[exp].pages = (struct page *)a;
      //一半return掉
      return b;
    }
  }
}

//如果single可以在zone[exp]里找到他的partner，那就把他们俩组合起来丢到更高层的zone去
//如果single没有在zone[exp]里找到partner，那就把single丢在zone[exp]里
static void combine(uint32_t exp, void *single) {
  assert(exp < sizeof(zone) / sizeof(struct zone));
  struct page *psingle = (struct page *)single;
  // partner可能是single+2^exp或single-2^exp
  // 看看能不能找到partner
  if (exp < sizeof(zone) / sizeof(struct zone) - 1 && zone[exp].pages != 0) {
    list_entry_t *p = (list_entry_t *)zone[exp].pages;
    bool end = false;
    do {
      int found; // 0=不是，1=single是前面的，2=single是后面的
      if ((uintptr_t)single + pow2(exp) * 4096 == (uintptr_t)p) {
        found = 1;
      } else if ((uintptr_t)single - pow2(exp) * 4096 == (uintptr_t)p) {
        found = 2;
      } else {
        found = 0;
      }
      if (found != 0) {
        //找到了partner
        //首先把p移出链表
        if (list_empty((list_entry_t *)zone[exp].pages)) {
          zone[exp].pages = 0;
        } else {
          list_del(p);
        }
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
  list_init((list_entry_t *)psingle);
  if (zone[exp].pages == 0) {
    zone[exp].pages = psingle;
  } else {
    list_add((list_entry_t *)zone[exp].pages, (list_entry_t *)psingle);
  }
}

void *kmem_page_alloc(size_t cnt) {
  cnt = next_pow2(cnt);
  return split(naive_log2(cnt));
}

void kmem_page_free(void *p, size_t cnt) {
  cnt = next_pow2(cnt);
  combine(naive_log2(cnt), p);
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

void kmem_page_dump() {
  printf("kmem_page_dump\n");
  printf("****************\n");
  for (uint32_t i = 0; i < sizeof(zone) / sizeof(struct zone); i++) {
    printf("exponent:%02x pages:0x%08llx count:%02x", (int32_t)i,
           (int64_t)(uintptr_t)zone[i].pages,
           (int32_t)list_size((list_entry_t *)zone[i].pages));
#ifndef NDEBUG
    printf(" debug_cnt:%02x", (int32_t)zone[i].cnt);
#endif
    printf("\n");
  }
  printf("****************\n");
}
