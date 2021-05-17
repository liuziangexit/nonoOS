#ifndef __KERNEL_VIRTUAL_MEMORY_H__
#define __KERNEL_VIRTUAL_MEMORY_H__
#include <avlmini.h>
#include <list.h>
#include <stdbool.h>

// 首次使用前，对vm系统做一些检查
void virtual_memory_check();

enum virtual_memory_area_type { KERNEL, CODE, STACK, MALLOC };

/*
CAUTION!
compare_malloc_vma
entry2vma
依赖于此类型的内存布局
*/
// 表示一段虚拟内存
struct virtual_memory_area {
  struct avl_node avl_node;
  uintptr_t start; // 虚拟内存起点
  uint32_t size;   // 虚拟内存大小(字节)
  uint16_t flags;  // 页表里的flags
  enum virtual_memory_area_type type;
  // malloc类型时用的额外信息
  list_entry_t list_node; // 串在virtual_memory.partial或full
  list_entry_t
      free_area; // umalloc_free_area的链表，按照len从小到大排序，这样first-fit就是best-fit
  uint32_t max_free_area_len;          // 最大的freearea是多大
  struct avl_tree allocated_free_area; // 已分配的free_area信息
  void *physical;                      // 物理页地址
};

struct virtual_memory {
  // 所有vma
  struct avl_tree vma_tree;
  // 部分使用的malloc类型vma
  // 按照max_free_area_len从小到大排序
  list_entry_t partial;
  // 全部使用的malloc类型vma
  list_entry_t full;
  // uint32_t vma_cnt;
  uint32_t *page_directory;
};

// 初始化一个虚拟地址空间结构
struct virtual_memory *virtual_memory_create();
//从一个已有的页目录里建立vma
void virtual_memory_clone(struct virtual_memory *vm,
                          const uint32_t *page_directory,
                          enum virtual_memory_area_type type);
// 销毁一个虚拟地址空间结构
void virtual_memory_destroy(struct virtual_memory *vm);
// 寻找对应的vma，如果没有返回0
struct virtual_memory_area *virtual_memory_get_vma(struct virtual_memory *vm,
                                                   uint32_t mem);
// 在一个虚拟地址空间结构中寻找[begin,end)中空闲的指定长度的地址空间
// 返回0表示找不到
uintptr_t virtual_memory_find_fit(struct virtual_memory *vm, uint32_t vma_size,
                                  uintptr_t begin, uintptr_t end,
                                  uint16_t flags);

// 分配对齐到4k的虚拟内存，但没有对应的物理内存
// 如果vma_start紧接着一个已存在的vma，那么将不会创建新的vma，而是拓展已存在的vma
// 如果成功，返回vma。否则返回0
// ext_ctor是用来生成额外信息，并储存到ext指针里的函数。ext_dtor是virtual_memory_free时用于销毁ext指针用的
// ext_ctor的第一个函数是vma，第二个函数是已有的ext。
// 如果是创建新vma时调用，已有ext=0，如果是合并vma时调用，已有ext!=0
struct virtual_memory_area *
virtual_memory_alloc(struct virtual_memory *vm, uintptr_t vma_start,
                     uintptr_t vma_size, uint16_t flags,
                     enum virtual_memory_area_type type, bool merge);
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

/*
CAUTION!
compare_free_area
依赖于此类型的内存布局
*/
//存在物理页首部的链表头
struct umalloc_free_area {
  union {
    list_entry_t list_head;
    struct avl_node avl_head;
  };
  uintptr_t addr;
  size_t len;
};

uintptr_t umalloc(struct virtual_memory *vm, uint32_t size);
void umalloc_pgfault(struct virtual_memory *vm,
                     struct virtual_memory_area *vma);
void ufree(struct virtual_memory *vm, uintptr_t addr);

#endif
