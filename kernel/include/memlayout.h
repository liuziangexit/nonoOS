#ifndef __KERNEL_MEMLAYOUT_H
#define __KERNEL_MEMLAYOUT_H

/*
注意，这里的物理地址就是线性地址，因为我们有平坦的segementation
Physical Address        |        Virtual Address        |
[0x7C00, 0x7E00)        |    [0xC0007C00, 0xC0007E00)   | bootloader代码，512字节
[0x0, 0x100000)         |    [0xC0000000, 0xC0100000)   | 保留区域，1MB
[0x100000, 0x900000)    |    [0xC0100000, 0xC0900000)   | 内核代码区域，8MB
[0x900000, 0x901000)    |    [0xC0900000, 0xC0901000)   | 内核页目录，4KB
[0x904000, 0x906000)    |    [0xC0904000, 0xC0908000)   | 内核栈，16KB（这个地址对齐到16KB，因为栈好像有这个对齐要求...我也不知道是不是真的，反正我们也不差这点内存）
[0x908000, 内存上限)     |                               |  kmalloc区域
*/

//低1MB是保留区域
#define PRESERVED 0x100000
//内核虚拟基地址
#define KERNEL_VIRTUAL_BASE 0xC0000000
//内核被link到的虚拟地址
#define KERNEL_LINKED (KERNEL_VIRTUAL_BASE + PRESERVED)
//虚拟地址转物理地址
#define V2P(n) (n - KERNEL_VIRTUAL_BASE)
//物理地址转虚拟地址
#define P2V(n) (n + KERNEL_VIRTUAL_BASE)

//内核栈位置(x86栈向低地址增长)
#define KERNEL_STACK 0x906000
//内核页目录位置
#define KERNEL_PGDIR 0x900000
//内核kmalloc区域
#define KERNEL_FREESPACE 0x908000

// Page directory and page table constants.
// https://www.youtube.com/watch?v=jkGZDb3100Q&t=867s
#define PGD_ENTRIES 1024   // # directory entries per page directory
#define PTE_ENTRIES 1024 // # PTEs per page table
#define PGSIZE 4096          // bytes mapped by a page

#endif
