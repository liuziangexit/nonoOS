#ifndef __KERNEL_MEMLAYOUT_H
#define __KERNEL_MEMLAYOUT_H

#define KERNEL_VIRTUAL_BASE 0xC0100000
#define V2P(n) (n - 0xC0000000)
#define KERNEL_STACK_SIZE 0x2000


// Page directory and page table constants.
// https://www.youtube.com/watch?v=jkGZDb3100Q&t=867s
#define PGDIR_ENTRIES      1024    // # directory entries per page directory
#define PGTABLE_ENTRIES      1024    // # PTEs per page table
#define PGSIZE          4096    // bytes mapped by a page

#endif
