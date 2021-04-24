#include <avlmini.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <virtual_memory.h>

/*
本文件主要包含用户进程虚拟地址空间管理
*/

int vma_compare(const void *a, const void *b) {
  const struct virtual_memory_area *ta = (const struct virtual_memory_area *)a;
  const struct virtual_memory_area *tb = (const struct virtual_memory_area *)b;
  return ta->vma_start - tb->vma_start;
}

//初始化一个虚拟地址空间结构
struct virtual_memory *virtual_memory_create() {
  struct virtual_memory *vm = malloc(sizeof(struct virtual_memory));
  if (!vm) {
    return 0;
  }
  vm->page_directory = kmem_page_alloc(1);
  if (!vm->page_directory) {
    free(malloc);
    return 0;
  }
  memset(vm->page_directory, 0, _4K);
  avl_tree_init(&vm->vma_tree, vma_compare, sizeof(struct virtual_memory_area),
                0);
  return vm;
}

//销毁一个虚拟地址空间结构
void virtual_memory_destroy(struct virtual_memory *vm) {
  assert(vm);
  //遍历页目录表，释放掉里面引用的页表
  for (uint32_t *p = vm->page_directory; p < vm->page_directory + 1024; p++) {
    uint32_t entry = *p;
    //如果一个PDE presented，并且不是大页，那肯定就是引用了一个PT
    if (entry & PTE_P && (entry & PTE_PS) == 0) {
      //移除flags，得到页表地址
      void *page_table = (void *)(entry | ~(uint32_t)0xFFF);
      kmem_page_free(page_table, 1);
    }
  }
  kmem_page_free(vm->page_directory, 1);
  //遍历二叉树，释放掉节点
  //本来我还以为要自己写后序遍历，没想到云老师已经做了这个需求，祝他长命百岁
  avl_tree_clear(&vm->vma_tree, free);
  //最后释放vm结构
  free(vm);
}

//在一个虚拟地址空间结构中进行以4k为边界映射
//返回false如果指定的虚拟地址已经有映射了
bool virtual_memory_map(struct virtual_memory *vm, uintptr_t vma_start,
                        uintptr_t vma_size);
void virtual_memory_unmap(struct virtual_memory *vm, uintptr_t vma_start);
//在一个虚拟地址空间结构中寻找空闲的指定长度的地址空间
//返回0表示找不到
uintptr_t virtual_memory_find(struct virtual_memory *vm, uint32_t vma_size);

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
