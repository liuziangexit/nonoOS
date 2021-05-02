#ifndef __KERNEL_VIRTUAL_MEMORY_H__
#define __KERNEL_VIRTUAL_MEMORY_H__
#include <avlmini.h>
#include <stdbool.h>

// 表示一段虚拟内存
struct virtual_memory_area {
  struct avl_node avl_node;
  uintptr_t vma_start;
  uint32_t vma_size; // in bytes
};

struct virtual_memory {
  struct avl_tree vma_tree;
  // uint32_t vma_cnt;
  uint32_t *page_directory;
};

// 初始化一个虚拟地址空间结构
struct virtual_memory *virtual_memory_create();
//从一个已有的页目录里建立vma
void virtual_memory_clone(struct virtual_memory *vm,
                          const uint32_t *page_directory);
// 销毁一个虚拟地址空间结构
void virtual_memory_destroy(struct virtual_memory *vm);
// 寻找对应的vma，如果没有返回0
// TODO这个不应该是vma_start而应该是任何内存地址，就能找到对应的vma
struct virtual_memory_area *virtual_memory_get_vma(struct virtual_memory *vm,
                                                   uint32_t vma_start);
// 在一个虚拟地址空间结构中进行以4k为边界映射
// 返回false如果指定的虚拟地址已经有映射了
bool virtual_memory_map(struct virtual_memory *vm, uintptr_t vma_start,
                        uintptr_t vma_size, uintptr_t physical_addr,
                        uint16_t flags);
void virtual_memory_unmap(struct virtual_memory *vm, uintptr_t vma_start);
// 在一个虚拟地址空间结构中寻找[begin,end)中空闲的指定长度的地址空间
// 返回0表示找不到
uintptr_t virtual_memory_find_fit(struct virtual_memory *vm, uint32_t vma_size,
                                  uintptr_t begin, uintptr_t end);
bool virtual_memory_free(struct virtual_memory *vm, void *);

#endif
