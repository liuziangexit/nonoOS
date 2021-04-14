#include <assert.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <panic.h>
#include <stdbool.h>
#include <string.h>
// entry.S中使用的页目
// 页目和页表必须对齐到页边界
// PTE_PS in a page directory entry enables 4Mbyte pages.

_Alignas(4096) uint32_t kernel_page_directory[1024] = {
    // Map Virtual's [0, 4MB) to Physical's [0, 4MB)
    [0] = 0x0 | PTE_P | PTE_W | PTE_PS,

    // 映射内核地址开始的8MB空间
    // Map Virtual's [KERNBASE, KERNBASE + 4MB) to Physical's [0, 4MB)
    [KERNEL_VIRTUAL_BASE >> PDXSHIFT] = 0x0 | PTE_P | PTE_W | PTE_PS,
    // Map Virtual's [KERNBASE + 4MB, KERNBASE + 8MB) to Physical's [4MB, 8MB)
    [(KERNEL_VIRTUAL_BASE + 0x400000) >> PDXSHIFT] = 0x400000 | PTE_P | PTE_W |
                                                     PTE_PS,
    // 映射4M的内核栈
    [(KERNEL_VIRTUAL_BASE + KERNEL_STACK) >> PDXSHIFT] = KERNEL_STACK | PTE_P |
                                                         PTE_W | PTE_PS
    //
};

// map大页
void pd_map_4M(void *pd, uintptr_t linear, uintptr_t physical, uint32_t pgcnt,
               uint32_t flags) {
  assert(((uintptr_t)pd) % 4096 == 0);
  assert(physical % _4M == 0 && linear % _4M == 0);
  assert(linear / _4M + pgcnt < 1024);
  assert(physical / _4M + pgcnt < 1024);
  assert(flags >> 12 == 0);
  assert((flags & PTE_PS) == PTE_PS);

  uint32_t *entry = (uint32_t *)(pd + linear / _4M * 4);
  union {
    struct PDE4M pde;
    uint32_t val;
  } pde;
  for (uint32_t i = 0; i < pgcnt; i++) {
    pde.val = 0;
    set_pde4m(&pde.pde, (physical + i * _4M), flags);
    *(entry + i) = pde.val;
  }
}

// map小页
void pd_map_4K(void *pt, uintptr_t linear, uintptr_t physical, uint32_t pgcnt,
               uint32_t flags) {
  const uint32_t pt_idx = (0x3FFFFF & linear) >> 12;

  assert(((uintptr_t)pt) % 4096 == 0);
  assert(physical % _4K == 0 && linear % _4K == 0);
  assert(pt_idx + pgcnt < 1024);
  assert(physical / _4K + pgcnt < 1024 * 1024);
  assert(flags >> 12 == 0);

  uint32_t *entry = (uint32_t *)(pt + pt_idx * 4);
  union {
    struct PTE4K pte;
    uint32_t val;
  } pte;
  for (uint32_t i = 0; i < pgcnt; i++) {
    pte.val = 0;
    set_pte4k(&pte.pte, (physical + i * _4K), flags);
    *(entry + i) = pte.val;
  }
}

uint32_t pd_value(void *pd, uintptr_t linear) {
  assert(((uintptr_t)pd) % 4096 == 0);
  linear = ROUNDDOWN(linear, _4M);
  uint32_t *entry = (uint32_t *)(pd + linear / _4M * 4);
  return *entry;
}
