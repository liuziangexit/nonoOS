#ifndef __KERNEL_VIRTUAL_MEMORY_H__
#define __KERNEL_VIRTUAL_MEMORY_H__
#include <avlmini.h>

// struct virtual_memory;

//表示一段虚拟内存
struct virtual_memory_area {
  struct avl_node avl_node;
  // struct virtual_memory *vm;
  uintptr_t vma_start;
  uint32_t vma_size;
  // uint16_t vma_flag;
};

struct virtual_memory {
  struct avl_tree vma;
  // uint32_t vma_cnt;
  uint32_t *page_directory;
};

#endif
