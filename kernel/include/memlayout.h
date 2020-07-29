#ifndef __KERNEL_MEMLAYOUT_H
#define __KERNEL_MEMLAYOUT_H

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
//内核栈大小
#define KERNEL_STACK_SIZE 0x2000

// Page directory and page table constants.
// https://www.youtube.com/watch?v=jkGZDb3100Q&t=867s
#define PGDIR_ENTRIES 1024   // # directory entries per page directory
#define PGTABLE_ENTRIES 1024 // # PTEs per page table
#define PGSIZE 4096          // bytes mapped by a page

#endif
