#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
// entry.S中使用的页目
// 页目和页表必须对齐到页边界
// PTE_PS in a page directory entry enables 4Mbyte pages.

// FIXME 现在为了测试方便暂时去掉ring限制，ring3就能访问全部页面。实现用户task后移除这个
// TODO 这里是kernel的页目录，要把0 - KERNEL_SIZE映射到3G - 3G+KERNEL_SIZE，
// 然后把KERNEL_SIZE - MEM_LIM映射到0 - (MEM_LIM-KERNEL_SIZE)

_Alignas(PGSIZE) int32_t kernel_page_directory[NPDENTRIES] = {
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
