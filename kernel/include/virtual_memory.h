#ifndef __KERNEL_VIRTUAL_MEMORY_H__
#define __KERNEL_VIRTUAL_MEMORY_H__
#include <avlmini.h>
#include <stdbool.h>

// 表示一段虚拟内存
struct virtual_memory_area {
  struct avl_node avl_node;
  uintptr_t vma_start;
  uint32_t vma_size; // in bytes
  uint16_t flags;    //页表里的flags
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
struct virtual_memory_area *virtual_memory_get_vma(struct virtual_memory *vm,
                                                   uint32_t mem);
// 在一个虚拟地址空间结构中寻找[begin,end)中空闲的指定长度的地址空间
// 返回0表示找不到
uintptr_t virtual_memory_find_fit(struct virtual_memory *vm, uint32_t vma_size,
                                  uintptr_t begin, uintptr_t end);

// 分配对齐到4k的虚拟内存，但没有对应的物理内存
// 如果vma_start紧接着一个已存在的vma，那么将不会创建新的vma，而是拓展已存在的vma
// 如果成功，返回vma。否则返回0
struct virtual_memory_area *virtual_memory_alloc(struct virtual_memory *vm,
                                                 uintptr_t vma_start,
                                                 uintptr_t vma_size,
                                                 uint16_t flags);
// 删除一个vma
void virtual_memory_free(struct virtual_memory *vm,
                         struct virtual_memory_area *vma);

// 在一个虚拟地址空间结构中进行以4k为边界映射
// 返回false如果
// 1)没有管理virtual_addr的vma
// 2)有vma但是size越界
// 3)这片地址中至少有一部分已经有映射了
// vma为空时，函数会自动寻找vma
bool virtual_memory_map(struct virtual_memory *vm,
                        struct virtual_memory_area *vma, uintptr_t virtual_addr,
                        uint32_t size, uintptr_t physical_addr);
void virtual_memory_unmap(struct virtual_memory *vm, uintptr_t virtual_addr,
                          uint32_t size);

#endif
