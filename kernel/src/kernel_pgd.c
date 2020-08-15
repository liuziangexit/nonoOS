#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
// entry.S中使用的页目
// 页目和页表必须对齐到页边界
// PTE_PS in a page directory entry enables 4Mbyte pages.

_Alignas(PGSIZE) int32_t kernel_page_directory[NPDENTRIES] = {
    // Map Virtual's [0, 4MB) to Physical's [0, 4MB)
    [0] = 0x0 | PTE_P | PTE_W | PTE_PS,

    // 8K映射
    // Map Virtual's [KERNBASE, KERNBASE + 4MB) to Physical's [0, 4MB)
    [KERNEL_VIRTUAL_BASE >> PDXSHIFT] = 0x0 | PTE_P | PTE_W | PTE_PS,
    // Map Virtual's [KERNBASE + 4MB, KERNBASE + 8MB) to Physical's [4MB, 8MB)
    [(KERNEL_VIRTUAL_BASE + 0x400000) >> PDXSHIFT] = 0x400000 | PTE_P | PTE_W |
                                                     PTE_PS,
    // 16K内核栈
    [(KERNEL_VIRTUAL_BASE + KERNEL_STACK) >> PDXSHIFT] = KERNEL_STACK | PTE_P |
                                                         PTE_W | PTE_PS,
    [(KERNEL_VIRTUAL_BASE + KERNEL_STACK + 0x400000) >>
        PDXSHIFT] = (KERNEL_STACK + 0x400000) | PTE_P | PTE_W | PTE_PS,
    [(KERNEL_VIRTUAL_BASE + KERNEL_STACK + 0x800000) >>
        PDXSHIFT] = (KERNEL_STACK + 0x800000) | PTE_P | PTE_W | PTE_PS,
    [(KERNEL_VIRTUAL_BASE + KERNEL_STACK + 0x1000000) >>
        PDXSHIFT] = (KERNEL_STACK + 0x1000000) | PTE_P | PTE_W | PTE_PS};
