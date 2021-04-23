#include <assert.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <panic.h>
#include <stdbool.h>
#include <string.h>

/*
本文件主要包含两个功能的实现
1.页表/页目录表的相关操作
2.用户进程虚拟地址空间管理
*/

// entry.S中使用的页目录表
// 页目和页表必须对齐到页边界(4k)
_Alignas(4096) uint32_t boot_pd[1024] = {
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
