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
#include <x86.h>

void kmem_init() {
  extern uint32_t kernel_page_directory[];
  //如果直接修改正在使用的页目录，会翻车
  //所以lcr3一个一模一样的页目录副本，这样我们才能开始修改真正的页目录
  _Alignas(4096) uint32_t temp_pd[1024];
  memcpy(temp_pd, kernel_page_directory, 4096);
  lcr3(V2P((uintptr_t)temp_pd));
  //好吧，现在开始修改真的页目录(kernel_page_directory)
  //虚拟地址0到4M -> 物理地址0到4M（为了CGA这样的外设地址）
  pd_map_ps(kernel_page_directory, 0, 0, 1, PTE_P | PTE_W | PTE_PS | PTE_U);
  //虚拟地址4M到3G -> 物理地址1G+4M到3G
  pd_map_ps(kernel_page_directory, _4M, 1024 * 1024 * 1024 + _4M, 768 - 1,
            PTE_P | PTE_W | PTE_PS | PTE_U);
  //虚拟地址3G到4G -> 物理地址0到1G
  pd_map_ps(kernel_page_directory, KERNEL_VIRTUAL_BASE, 0, 256,
            PTE_P | PTE_W | PTE_PS | PTE_U);
  //重新加载真的页目录(kernel_page_directory)
  lcr3(V2P((uintptr_t)kernel_page_directory));
}

struct page {
  struct list_entry li;
};

struct zone {
  struct page *pages;
  uint32_t cnt;
#ifndef NDEBUG
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
#ifndef NDEBUG
  for (uint32_t i = 0; i < sizeof(zone) / sizeof(struct zone); i++) {
    zone[i].exp = i;
  }
#endif

  for (int i = 0; i < memlayout->count; i++) {
    //为硬件保留的内存
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
      printf("kmem_page_init: address at e820[%d] is smaller than a page, "
             "ignore\n",
             i);
#endif
      continue;
    }
    //高于4G的内存，只可远观不可亵玩焉
    if (memlayout->ard[i].addr > 0xfffffffe) {
#ifndef NDEBUG
      printf("kmem_page_init: address at e820[%d] too high, ignore\n", i);
#endif
      continue;
    }

    char *addr = (char *)(uintptr_t)memlayout->ard[i].addr;
    int32_t page_count = (int32_t)(memlayout->ard[i].size / 4096);
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

      //对于addr+c*4096越过4G界的处理
      if (0xfffffffe - c * 4096 < (uint32_t)addr) {
        c = (0xfffffffe - (uint32_t)addr) / 4096;
        if (c == 0) {
#ifndef NDEBUG
          printf(
              "kmem_page_init: address %08llx at e820[%d] too high, ignore\n",
              (int64_t)(uintptr_t)addr, i);
#endif
          break;
        }
      }

#ifndef NDEBUG
      printf("%d ", c);
#endif

      struct page *pg = (struct page *)addr;
      list_init((list_entry_t *)pg);
      const uint32_t log2_c = naive_log2(c);
      if (!(zone[log2_c].pages)) {
        zone[log2_c].pages = pg;
      } else {
        list_add((list_entry_t *)(zone[log2_c].pages), (list_entry_t *)pg);
      }
      zone[log2_c].cnt++;
      page_count -= c;
      addr += (4096 * c);
      assert(((uint32_t)(zone[log2_c].pages) + zone[log2_c].cnt) <= 0xfffffffe);
    }
#ifndef NDEBUG
    printf("\n");
#endif
  }
#ifndef NDEBUG
  printf("kmem_page_init: OK\n");
#endif
}
void *kmem_page_alloc(size_t cnt) {
  UNUSED(cnt);
  return 0;
}
void kmem_page_free(void *p, size_t cnt) {
  UNUSED(p);
  UNUSED(cnt);
}
void kmem_page_dump() {}
