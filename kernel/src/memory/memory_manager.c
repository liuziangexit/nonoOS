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

struct Page {
  struct list_entry li;
  uint32_t pg_cnt;
#ifndef NDEBUG
  uint32_t exp;
#endif
};

// 2^0到2^30
static struct Page *zone[31];

static inline bool is_pow2(uint32_t x) { return !(x & (x - 1)); }

//找到第一个大于等于x的2的幂
// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
// 实际上可以用bsrq这样的机器指令来实现更快的版本，但是算了
static uint32_t next_pow2(uint32_t x) {
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
static inline uint32_t prev_pow2(uint32_t x) { return next_pow2(x) / 2; }

void kmem_page_init(struct e820map_t *memlayout) {
  //for()
}
void *kmem_page_alloc(size_t cnt);
void kmem_page_free(void *, size_t cnt);
void kmem_page_dump();
