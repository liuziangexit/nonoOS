#include <memlayout.h>
#include <mmu.h>
#include <stdint.h>

// entry.S中使用的页目录表
// 页目和页表必须对齐到页边界(4k)
_Alignas(4096) uint32_t kernel_pd[1024] = {
    //为了bootloader栈
    [0] = 0x0 | PTE_P | PTE_W | PTE_PS,
    //为了entry.S打开分页后的代码
    [KERNEL_IMAGE >> PDXSHIFT] = KERNEL_IMAGE | PTE_P | PTE_W | PTE_PS,
    //为了在kentry
    [(KERNEL_VIRTUAL_BASE + KERNEL_IMAGE) >> PDXSHIFT] = KERNEL_IMAGE | PTE_P |
                                                         PTE_W | PTE_PS
    //
};
