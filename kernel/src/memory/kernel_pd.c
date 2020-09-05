#include <assert.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
// entry.S中使用的页目
// 页目和页表必须对齐到页边界
// PTE_PS in a page directory entry enables 4Mbyte pages.

// FIXME
// 现在为了测试方便暂时去掉ring限制，ring3就能访问全部页面。实现用户task后移除这个
// TODO 这里是kernel的页目录，要把0 - KERNEL_SIZE映射到3G - 3G+KERNEL_SIZE，
// 然后把KERNEL_SIZE - MEM_LIM映射到0 - (MEM_LIM-KERNEL_SIZE)

_Alignas(PGSIZE) uint32_t kernel_page_directory[1024] = {
    // Map Virtual's [0, 4MB) to Physical's [0, 4MB)
    [0] = 0x0 | PTE_P | PTE_W | PTE_PS | PTE_U,

    // 映射内核地址开始的8MB空间
    // Map Virtual's [KERNBASE, KERNBASE + 4MB) to Physical's [0, 4MB)
    [KERNEL_VIRTUAL_BASE >> PDXSHIFT] = 0x0 | PTE_P | PTE_W | PTE_PS | PTE_U,
    // Map Virtual's [KERNBASE + 4MB, KERNBASE + 8MB) to Physical's [4MB, 8MB)
    [(KERNEL_VIRTUAL_BASE + 0x400000) >> PDXSHIFT] = 0x400000 | PTE_P | PTE_W |
                                                     PTE_PS | PTE_U,
    // 映射4M的内核栈
    [(KERNEL_VIRTUAL_BASE + KERNEL_STACK) >> PDXSHIFT] = KERNEL_STACK | PTE_P |
                                                         PTE_W | PTE_PS | PTE_U
    //
};

_Alignas(PGSIZE) uint32_t kernel_page_directory_test[1024];

#define _4K (4096)
#define _4M (_4K * 1024)

// map大页
void pd_map_ps(void *pd, uintptr_t linear, uintptr_t physical, uint32_t pgcnt,
               uint32_t flags) {
  assert(((uintptr_t)pd) % 4096 == 0);
  assert(physical % _4M == 0 && linear % _4M == 0);
  assert(linear / _4M + pgcnt < 1024);
  assert(flags >> 12 == 0);
  assert((flags & PTE_PS) == PTE_PS);

  uint32_t *entry = (uint32_t *)(pd + linear / _4M * 4);
  for (uint32_t i = 0; i < pgcnt; i++) {
    *(entry + i) = (physical + i * _4M) | flags;
  }
}
