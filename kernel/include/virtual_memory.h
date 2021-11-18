#ifndef __KERNEL_VIRTUAL_MEMORY_H__
#define __KERNEL_VIRTUAL_MEMORY_H__
#include <avlmini.h>
#include <compiler_helper.h>
#include <list.h>
#include <stdbool.h>
#include <stdlib.h>

// 首次使用前，对vm系统做一些检查
void virtual_memory_check();

enum virtual_memory_area_type {
  SHM,     // 共享内存
  UCODE,   // 用户代码
  USTACK,  // 用户栈
  UMALLOC, // 用户malloc内存块
  UKERNEL, // 用户程序看来的内核空间
  KUSER,   // 内核看来的用户空间
  KDMA,    // 内核DMA区
  KCODE,   // 内核代码
  KNORMAL, // 内核NORMAL REGION
  KMAP     // 内核MAP REGION
};

static __always_inline const char *
vma_type_str(enum virtual_memory_area_type e) {
  if (e == SHM)
    return "SHM    ";
  if (e == UCODE)
    return "UCODE  ";
  if (e == USTACK)
    return "USTACK ";
  if (e == UMALLOC)
    return "UMALLOC";
  if (e == UKERNEL)
    return "UKERNEL";
  if (e == KUSER)
    return "KUSER  ";
  if (e == KDMA)
    return "KDMA   ";
  if (e == KCODE)
    return "KCODE  ";
  if (e == KNORMAL)
    return "KNORMAL";
  if (e == KMAP)
    return "KMAP   ";
  abort();
  __builtin_unreachable();
}

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
  // TODO 下面应该改成union

  // malloc类型时用的额外信息
  list_entry_t list_node; // 串在virtual_memory.partial或full
  list_entry_t
      free_area_sort_by_len; // umalloc_free_area的链表，按照len从小到大排序，这样first-fit就是best-fit
  struct avl_tree
      free_area_sort_by_addr; // umalloc_free_area的二叉树，以addr为主键
  uint32_t max_free_area_len;          // 最大的freearea是多大
  struct avl_tree allocated_free_area; // 已分配的free_area信息
  uintptr_t physical;                  // 物理页地址

  // 共享内存类型时用的额外信息
  uint32_t shid; // shared memory id
};

struct virtual_memory {
  // 所有vma
  struct avl_tree vma_tree;
  // 部分使用的malloc类型vma
  // 按照max_free_area_len从小到大排序
  list_entry_t partial;
  // 全部使用的malloc类型vma
  list_entry_t full;
  uint32_t *page_directory;
};

// 初始化一个虚拟地址空间结构
struct virtual_memory *virtual_memory_create();
void virtual_memory_init(struct virtual_memory *vm, void *pd);
// 当前的vm
struct virtual_memory *virtual_memory_current();
//从一个已有的页目录里建立vma
void virtual_memory_clone(struct virtual_memory *vm,
                          const uint32_t *page_directory,
                          enum virtual_memory_area_type type);
// 销毁一个虚拟地址空间结构
void virtual_memory_destroy(struct virtual_memory *vm);
// 寻找对应的vma，如果没有返回0
struct virtual_memory_area *virtual_memory_get_vma(struct virtual_memory *vm,
                                                   uintptr_t mem);
// 在一个虚拟地址空间结构中寻找[begin,end)中空闲的指定长度的地址空间
// 返回0表示找不到
uintptr_t virtual_memory_find_fit(struct virtual_memory *vm, uint32_t vma_size,
                                  uintptr_t begin, uintptr_t end,
                                  uint16_t flags,
                                  enum virtual_memory_area_type type);

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
// vma为空时，函数会自动寻找vma
void virtual_memory_map(struct virtual_memory *vm,
                        struct virtual_memory_area *vma, uintptr_t virtual_addr,
                        uint32_t size, uintptr_t physical_addr);
void virtual_memory_unmap(struct virtual_memory *vm, uintptr_t virtual_addr,
                          uint32_t size);

// 打印
void virtual_memory_print(struct virtual_memory *vm);

/*
CAUTION!
compare_free_area
依赖于此类型的内存布局
*/
struct umalloc_free_area {
  struct avl_node avl_head;
  list_entry_t list_head;
  uintptr_t addr;
  size_t len;
};

uintptr_t umalloc(struct virtual_memory *vm, uint32_t size, bool lazy_map,
                  struct virtual_memory_area **out_vma,
                  uintptr_t *out_physical);
void upfault(struct virtual_memory *vm, struct virtual_memory_area *vma);
void ufree(struct virtual_memory *vm, uintptr_t addr);

// 共享内存
// 使用引用计数标记有多少个进程正在访问，当此值归0时(进程退出或显式调用shmunmap)，共享内存被删除
// 目前不支持设置访问权限等复杂的功能，每个共享内存都可能被任何权限的任何进程访问
// 不考虑攻击者恶意利用共享内存的情况

// kernel_object里的get_counter依赖于此类型的内存布局
struct shared_memory {
  struct avl_node head;
  uint32_t id;
  uint32_t ref; // 引用计数
  uintptr_t physical;
  uint32_t pgcnt;
};

// 创建共享内存，返回共享内存id
// 若返回0表示失败
uint32_t shared_memory_create(size_t size);
void shared_memory_destroy(struct shared_memory *);
// 获得共享内存的上下文
struct shared_memory *shared_memory_ctx(uint32_t id);
// map共享内存到当前地址空间
// map时会自动ref内核对象，unmap时会自动unref
void *shared_memory_map(uint32_t id, void *addr);
void shared_memory_unmap(void *addr);

#endif
