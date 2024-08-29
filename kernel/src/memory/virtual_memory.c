#include "../../include/task.h"
#include <atomic.h>
#include <avlmini.h>
#include <defs.h>
#include <kernel_object.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <panic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sync.h>
#include <virtual_memory.h>
#include <x86.h>

/*
本文件主要包含用户进程虚拟地址空间管理
TODO 合并邻近的VMA，小页表全部存的是物理地址，支持大小页混合的情况
-合并VMA
-合并页表为4M大页
*/

// #define VERBOSE

// 这个函数只会用来比较同vm里的vma，不会用来比较不同vm里的vma
int vma_compare(const void *a, const void *b) {
  const struct virtual_memory_area *ta = (const struct virtual_memory_area *)a;
  const struct virtual_memory_area *tb = (const struct virtual_memory_area *)b;
  if (ta->start > tb->start)
    return 1;
  if (ta->start < tb->start)
    return -1;
  if (ta->start == tb->start)
    return 0;
  __unreachable;
}

// 初始化一个虚拟地址空间结构
struct virtual_memory *virtual_memory_create() {
  struct virtual_memory *vm = malloc(sizeof(struct virtual_memory));
  if (!vm) {
    return 0;
  }
  void *pd = kmem_page_alloc(1);
  if (!pd) {
    free(vm);
    return 0;
  }
  memset(pd, 0, _4K);
  virtual_memory_init(vm, pd);
  return vm;
}

void virtual_memory_init(struct virtual_memory *vm, void *pd) {
  assert(vm);
  vm->page_directory = pd;
  avl_tree_init(&vm->vma_tree, vma_compare, sizeof(struct virtual_memory_area),
                0);
  list_init(&vm->full);
  list_init(&vm->partial);
}

// 从一个已有的页目录里建立vma
void virtual_memory_clone(struct virtual_memory *vm,
                          const uint32_t *page_directory,
                          enum virtual_memory_area_type type) {
  assert(((uintptr_t)page_directory) % _4K == 0);
  for (uint32_t pd_idx = 0; pd_idx < 1024; pd_idx++) {
    if (page_directory[pd_idx] & PTE_P) {
      // 对于每个PDE
      if (page_directory[pd_idx] & PTE_PS) {
        // 4M页
        uintptr_t ps_page_frame =
            (uintptr_t)(page_directory[pd_idx] & 0xFFC00000);
        struct virtual_memory_area *vma = virtual_memory_alloc(
            vm, pd_idx * _4M, _4M, page_directory[pd_idx] & 7, type, true);
        assert(vma);
        virtual_memory_map(vm, vma, pd_idx * _4M, _4M, ps_page_frame);
      } else {
        // 4K页
        uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & ~0xFFF);
        pt = (uint32_t *)P2V((uintptr_t)pt);
        for (uint32_t pt_idx = 0; pt_idx < 1024; pt_idx++) {
          if (pt[pt_idx] & PTE_P) {
            uintptr_t page_frame = (uintptr_t)(pt[pt_idx] & ~0xFFF);
            struct virtual_memory_area *vma =
                virtual_memory_alloc(vm, pt_idx * _4K + pd_idx * _4M, _4K,
                                     pt[pt_idx] & 7, type, true);
            assert(vma);
            virtual_memory_map(vm, vma, pt_idx * _4K + pd_idx * _4M, _4K,
                               page_frame);
          }
        }
      }
    }
  }
}

static void malloc_vma_destroy(struct virtual_memory_area *vma) {
  assert(vma->type == UMALLOC);
  list_del(&vma->list_node);
  list_init(&vma->list_node);
  assert(vma->size % 4096 == 0);
  if (vma->physical)
    free_region_page_free(vma->physical, vma->size / 4096);
}

static struct virtual_memory *__vm;
static void vm_clear_tree_callback(void *data) {
  struct virtual_memory_area *tdata = (struct virtual_memory_area *)data;
  switch (tdata->type) {
  case UMALLOC: {
    malloc_vma_destroy(tdata);
  } break;
  case SHM:
  case UKERNEL:
  case UCODE:
  case USTACK:
    break;
  default:
    panic("missing switch branch!");
  }
  virtual_memory_free(__vm, tdata);
}

// 销毁一个虚拟地址空间结构
void virtual_memory_destroy(struct virtual_memory *vm) {
  assert(vm);
  // 遍历页目录表，释放掉里面引用的页表
  for (uint32_t *p = vm->page_directory; p < vm->page_directory + 1024; p++) {
    uint32_t entry = *p;
    // 如果一个PDE presented，并且不是大页，那肯定就是引用了一个PT
    if (entry & PTE_P && (entry & PTE_PS) == 0) {
      // 移除flags，得到页表地址
      uintptr_t page_table = entry & ~(uint32_t)0xFFF;
      kmem_page_free((void *)P2V(page_table), 1);
    }
  }
  kmem_page_free(vm->page_directory, 1);
  // 遍历二叉树，释放掉节点
  // 本来我还以为要自己写后序遍历，没想到作者已经做了这个需求，祝他长命百岁
  {
    // 因为这里用一个static变量来模拟参数绑定，所以要关调度
    SMART_CRITICAL_REGION
    __vm = vm;
    avl_tree_clear(&vm->vma_tree, vm_clear_tree_callback);
  }
  // 最后释放vm结构
  free(vm);
}

// 寻找对应的vma，如果没有返回0
struct virtual_memory_area *virtual_memory_get_vma(struct virtual_memory *vm,
                                                   uintptr_t mem) {
  struct virtual_memory_area vma;
  vma.start = mem;
  struct virtual_memory_area *nearest =
      (struct virtual_memory_area *)avl_tree_nearest(&vm->vma_tree, &vma);
  if (nearest->start <= mem) {
    // 如果最近的在之前，那么可以直接判断mem在不在其中
    if (nearest->start + nearest->size > mem)
      return nearest;
  } else {
    // nearest->start > mem 情况
    // 如果最近的在之后，那么我们要找到它的前一个
    struct virtual_memory_area *prev =
        (struct virtual_memory_area *)avl_tree_prev(&vm->vma_tree, nearest);
    // nearest的前一个要不然为0，要不然在mem之前
    assert(prev == 0 || prev->start <= mem);
    // 判断mem在不在前一个之中
    if (prev != 0 && prev->start + prev->size > mem) {
      return prev;
    }
  }
  return 0;
}

static uintptr_t vm_get_4mboundary(uintptr_t addr) {
  return ROUNDDOWN(addr, 4 * 1024 * 1024);
}

// vma是否至少有一部分在boundary所指代的4m页中
static bool vm_same_4mpage(uint64_t boundary, struct virtual_memory_area *vma) {
  assert(boundary % _4M == 0);
  // 特殊情况，如果头在4m之前，尾在4m之后，那么还是返回true
  if (vma->start <= boundary && vma->start + vma->size >= boundary + _4M) {
    return true;
  }
  // 普通情况，只要头在4m里面，或者尾在4m里面，就返回true
  if ((vma->start >= boundary && vma->start < boundary + _4M) ||
      (vma->start + vma->size > boundary &&
       vma->start + vma->size <= boundary + _4M)) {
    return true;
  }
  return false;
}

// 比较两个pte或pde的flags
static bool vm_compare_flags(uint16_t a, uint16_t b) {
  a = a & 0x1E;
  b = b & 0x1E;
  return a == b;
}

// 返回begin，说明begin到begin+size是匹配flags的
// 返回another_begin，说明another_begin+size是匹配flags的，并且another_begin在[begin,end)中
// 返回0，说明无法匹配flags
uintptr_t vm_verify_area_flags(struct virtual_memory *vm, const uintptr_t begin,
                               const uintptr_t end, uint32_t size,
                               uint16_t flags,
                               enum virtual_memory_area_type type) {
  if (begin == end)
    return 0;
  assert(end > begin && end - begin >= size);
  uintptr_t real_begin = begin;
  struct virtual_memory_area key;
  key.start = vm_get_4mboundary(real_begin);
  while (true) {
    struct virtual_memory_area *vma = avl_tree_nearest(&vm->vma_tree, &key);
    if (!vma) {
      // 如果没有附近的vma，那好了
      break;
    }
    if (vm_same_4mpage(key.start, vma)) {
    SAME_PAGE:
      if (!vm_compare_flags(flags, vma->flags) || type != vma->type) {
        // 是同页但比不上
        // 将begin移动到下一个4M页开头
        real_begin = vm_get_4mboundary(real_begin + _4M);
        if (real_begin >= end || end - real_begin < size) {
          // 不够了
          return 0;
        }
        key.start = real_begin;
        continue;
      } else {
      MATCH:
        // 是同页并且匹配
        // 去检测下一个4M页
        if ((uint64_t)key.start + _4M >= 0xffffffff) {
          // 跑完地址空间了
          break;
        }
        key.start += _4M;
        if (key.start >= end) {
          // 检测完啦，没问题哈
          break;
        }
        continue;
      }
    } else {
      // 不是同页
      if (vma->start < key.start) {
        // 如果vma是此页之前，那么找到一个之后的vma，它有可能是同页
        vma = avl_tree_next(&vm->vma_tree, vma);
        if (!vma || !vm_same_4mpage(key.start, vma)) {
          // 然而还是没有同页的vma，说明这一页没问题啦
          goto MATCH;
        } else {
          // 确实有同页vma，跑去检测吧
          goto SAME_PAGE;
        }
      } else {
        // 本页没有任何vma，说明这一页没问题啦
        goto MATCH;
      }
    }
    abort();
    __unreachable;
  }
  return real_begin;
}

// 在一个虚拟地址空间结构中寻找[begin,end)中空闲的指定长度的地址空间
// 其实就是first-fit
// 返回0表示找不到
// 这是对齐到4K的
uintptr_t virtual_memory_find_fit(struct virtual_memory *vm, uint32_t vma_size,
                                  uintptr_t begin, uintptr_t end,
                                  uint16_t flags,
                                  enum virtual_memory_area_type type) {
  assert(end > begin && vma_size >= _4K && vma_size % _4K == 0 && vma_size > 0);
  uint64_t real_begin = ROUNDUP(begin, _4K), real_end = ROUNDDOWN(end, _4K);
  if (real_begin >= real_end || real_end - real_begin < vma_size) {
    return 0;
  }
  struct virtual_memory_area key;
  key.start = real_begin;
  struct virtual_memory_area *next = avl_tree_nearest(&vm->vma_tree, &key);
  if (next && next->start < real_begin) {
    // next是前一个
    if (next->start + next->size > real_begin) {
      // 如果前一个覆盖了realbegin的位置，那么要把real_begin向后移到它的结尾
      real_begin = ROUNDUP(next->start + next->size, 4096);
    }
    next = avl_tree_next(&vm->vma_tree, next);
  }
  // 遍历realbegin和realend之间的每个vma
  while (true) {
    assert(!next || next->start >= real_begin);
    if (next && next->start <= real_end) {
      // 这个next vma的开头在区间中
      // 现在我们召开民主大会，请大家给我们的候选人投票
      // 坏消息是，本次只有1名候选人参加选举；好消息是，你可以投反对票
      uint64_t end_candidate = ROUNDDOWN(next->start, 4096);
      if (end_candidate >= real_begin &&
          end_candidate - real_begin >= vma_size) {
        // 这个空闲区间满足长度要求，现在检测flags
        real_begin = vm_verify_area_flags(vm, real_begin, end_candidate,
                                          vma_size, flags, type);
        if (!real_begin) {
          // flags不匹配！
          // 其实现在这个处理性能不好，因为此时我可以直接跳到下一个4M页，从那里开始检测，对吧
          goto TRY_NEXT;
        }
        real_end = end_candidate;
        break;
      } else {
      // 如果next不够大，那么我们检测下一个空闲区间
      TRY_NEXT:
        real_begin = next->start + next->size;
        real_begin = ROUNDUP(real_begin, 4096);
        next = avl_tree_next(&vm->vma_tree, next);
        if (real_begin >= real_end) {
          return 0;
        }
        continue;
      }
    } else {
      // 如果没有next了，real_begin到real_end之间就可以用
      // 当然，有可能这空间不够大，函数的最后我们会检测这情况的
      // 检查flags先
      real_begin =
          vm_verify_area_flags(vm, real_begin, real_end, vma_size, flags, type);
      if (!real_begin) {
        // flags不匹配！
        return 0;
      }
      break;
    }
  }

  assert(real_begin % 4096 == 0 && real_end % 4096 == 0);
  if (real_end - real_begin >= vma_size) {
    assert(real_begin >= begin && real_begin < end && real_end > begin &&
           real_end <= end);
    return real_begin;
  }
  return 0;
}

struct virtual_memory_area *
virtual_memory_alloc(struct virtual_memory *vm, uintptr_t vma_start,
                     uintptr_t vma_size, uint16_t flags,
                     enum virtual_memory_area_type type, bool merge) {
  assert(vma_size > 0);
  // 我们只允许US、RW和P
  assert(flags >> 3 == 0);
  // 确定一下有没有重叠
  if (vma_start != virtual_memory_find_fit(vm, vma_size, vma_start,
                                           vma_start + vma_size, flags, type)) {
    panic("vma_start != virtual_memory_find_fit");
  }
  // 确认没有重叠，开始新增了
  // 首先获得vma_start之前最近的vma，叫他prev
  // TODO 看看这个操作是不是应该封装成函数，还经常用
  struct virtual_memory_area *prev = 0;
  if (merge) {
    struct virtual_memory_area key;
    key.start = vma_start;
    prev = avl_tree_nearest(&vm->vma_tree, &key);
    if (prev) {
      assert(prev->start != vma_start);
      if (prev->start > vma_start) {
        prev = avl_tree_prev(&vm->vma_tree, prev);
      }
    }
  }
  if (merge
      // 如果有前一个vma
      && prev
      // 并且前一个vma结尾正好是现在要加的vma_start
      && prev->start + prev->size == vma_start
      // 并且前一个vma的flags、type还与现在要加的vma相同
      && prev->flags == flags && prev->type == type) {
    // 那么直接拓展那个vma(根据设计，不应该拓展MALLOC类型的vma)
    assert(prev->type != UMALLOC);
    // 变大小
    prev->size += vma_size;
    return prev;
  } else {
    // 新增一个vma
    struct virtual_memory_area *vma =
        malloc(sizeof(struct virtual_memory_area));
    if (!vma) {
      return 0;
    }
    memset(vma, 0, sizeof(struct virtual_memory_area));
    avl_node_init(&vma->avl_node);
    vma->start = vma_start;
    vma->size = vma_size;
    vma->flags = flags;
    vma->type = type;
    if (avl_tree_add(&vm->vma_tree, vma)) {
      abort(); // never!
    }
#ifdef VERBOSE
    printf("virtual_memory_alloc type:%s  start:0x%09llx\n",
           vma_type_str(vma->type), (int64_t)vma->start);
#endif
    return vma;
  }
  abort();
  __unreachable;
}

static struct umalloc_free_area *new_free_area() {
  struct umalloc_free_area *p = malloc(sizeof(struct umalloc_free_area));
  memset(p, 0, sizeof(struct umalloc_free_area));
  return p;
}
static void delete_free_area(struct umalloc_free_area *ufa) { free(ufa); }

static struct umalloc_free_area *offset_free_area(void *list_head) {
  return list_head - 16;
}

static void unfreed_free_area_dtor(void *data) {
  struct umalloc_free_area *free_area = (struct umalloc_free_area *)data;
#ifdef VERBOSE
  printf("unfreed memory: 0x%09llx, size: %lld\n", (int64_t)free_area->addr,
         (int64_t)free_area->len);
#endif
  delete_free_area(free_area);
}

void virtual_memory_free(struct virtual_memory *vm,
                         struct virtual_memory_area *vma) {
  assert(vma);
#ifdef VERBOSE
  printf("virtual_memory_free  type:%s start:0x%09llx flags:"
         "%lld size:%lld\n",
         vma_type_str(vma->type), (int64_t)vma->start, (int64_t)vma->flags,
         (int64_t)vma->size);
#endif
#ifndef NDEBUG
  // debug时候确认下vma在vm里面
  struct virtual_memory_area *verify =
      (struct virtual_memory_area *)avl_tree_find(&vm->vma_tree, vma);
  assert(verify == 0 || verify == vma);
#endif
  if (vma->type == UMALLOC) {
    // 删除对应的物理内存也是umalloc_free做
    // 移除list_node的职责是umalloc_free来做，并且在做完之后就要list_init来标记
    if (!list_empty(&vma->list_node)) {
      abort();
    }
    // 销毁free_area结构
    for (list_entry_t *p = list_next(&vma->free_area_sort_by_len);
         p != &vma->free_area_sort_by_len;) {
      struct umalloc_free_area *del = offset_free_area(p);
      p = list_next(p);
      delete_free_area(del);
    }
    avl_tree_clear(&vma->allocated_free_area, unfreed_free_area_dtor);
  }
  // 从vma树中移除
  avl_tree_remove(&vm->vma_tree, vma);
  free(vma);
}

void virtual_memory_map(struct virtual_memory *vm,
                        struct virtual_memory_area *vma, uintptr_t virtual_addr,
                        uint32_t size, uintptr_t physical_addr) {
  assert(virtual_addr % 4096 == 0 && size % 4096 == 0 &&
         physical_addr % 4096 == 0 && size > 0);

  if (!vma) {
    vma = virtual_memory_get_vma(vm, virtual_addr);
    assert(vma);
  }
  // 确保va在vma中
  assert(virtual_addr >= vma->start && virtual_addr < vma->start + vma->size);
  // 确保virtual_addr+size没有越界
  assert(virtual_addr + size <= vma->start + vma->size);
  // 逐个在页目录里map
  // 在这个函数中，PTE和PDE的这些flags必须是同样的，也就是说参数flags不仅
  // 是新的PTE的flags，也必须和已有的PDE flags不冲突
  uint16_t flags = vma->flags;
  for (uint32_t p = virtual_addr; p < virtual_addr + size;
       p += _4K, physical_addr += _4K) {
    uint32_t pd_idx = p / _4M, pt_idx = 0x3FF & (p >> 12);
    uint32_t *pde = &vm->page_directory[pd_idx];
    // 现在开始处理页表
    if (((*pde) & PTE_P) == 0) {
      // PDE是空的，分配一个页表
      uint32_t *pt = kmem_page_alloc(1);
      // FIXME
      // 这里失败的时候，应该要回收本次函数调用已分配的结构，并且返回false
      // 所有调用本函数的地方都要修改
      assert(pt);
      memset(pt, 0, _4K);
      union {
        struct PDE_REF pde;
        uint32_t value;
      } punning;
      punning.value = 0;
      pde_ref(&punning.pde, V2P((uintptr_t)pt), flags);
      *pde = punning.value;
    } else {
      assert(((*pde) & PTE_PS) == 0);
      // PDE已经有一个页表了
      // 确定PDE的flags和现在要加的PTE flags是一样的
      assert((uint16_t)((*pde) & 0x1F) == flags);
    }
    // 确定页表没问题了，现在开始改页表
    uint32_t *pt = (uint32_t *)P2V((((uint32_t)*pde) & ~0xFFF));
    union {
      struct PTE pte;
      uint32_t value;
    } punning;
    punning.value = 0;
    pte_map(&punning.pte, physical_addr, flags);
    pt[pt_idx] = punning.value;
  }
}

void virtual_memory_unmap(struct virtual_memory *vm, uintptr_t virtual_addr,
                          uint32_t size) {
  assert(virtual_addr % 4096 == 0);
  assert(size % 4096 == 0);
  // 在页表里取消这些地址的map
  for (uintptr_t p = virtual_addr; p < virtual_addr + size; p += _4K) {
    uint32_t pd_idx = p / _4M, pt_idx = 0x3FF & (p >> 12);
    assert(vm->page_directory[pd_idx] & PTE_P);
    assert((vm->page_directory[pd_idx] & PTE_PS) == 0);
    uint32_t *pt =
        (uint32_t *)P2V((uintptr_t)(vm->page_directory[pd_idx] & ~0xFFF));
    pt[pt_idx] = 0;
  }
}

void virtual_memory_print(struct virtual_memory *vm) {
  printf("***************virtual_memory_print***************\n");
  for (struct virtual_memory_area *vma = avl_tree_first(&vm->vma_tree);
       vma != 0; vma = avl_tree_next(&vm->vma_tree, vma)) {
    printf("type:%s start:0x%08llx length:%lld(%lldMB)\n",
           vma_type_str(vma->type), (int64_t)vma->start, (int64_t)vma->size,
           (int64_t)vma->size / 1024 / 1024);
  }
  printf("**************************************************\n");
}

void virtual_memory_check() {
  if (sizeof(struct umalloc_free_area) > MAX_ALIGNMENT) {
    panic("sizeof(umalloc_free_area) > MAX_ALIGNMENT");
  }
}

// 如果你有virtual_memory_area::list_node的指针，用这函数去取得那个vma
static inline struct virtual_memory_area *entry2vma(list_entry_t *e) {
  return (struct virtual_memory_area *)(((uintptr_t)e) - 32);
}

static int compare_free_area_by_len(const void *a, const void *b) {
  const struct umalloc_free_area *ta = (const struct umalloc_free_area *)a;
  const struct umalloc_free_area *tb = (const struct umalloc_free_area *)b;
  if (ta->len > tb->len)
    return 1;
  else if (ta->len < tb->len)
    return -1;
  else
    return 0;
}

static int compare_free_area_by_addr(const void *a, const void *b) {
  const struct umalloc_free_area *ta = (const struct umalloc_free_area *)a;
  const struct umalloc_free_area *tb = (const struct umalloc_free_area *)b;
  if (ta->addr > tb->addr)
    return 1;
  else if (ta->addr < tb->addr)
    return -1;
  else
    return 0;
}

static int compare_malloc_vma(const void *a, const void *b) {
  const struct virtual_memory_area *ta = ((void *)a);
  const struct virtual_memory_area *tb = ((void *)b);
  assert(ta->type == UMALLOC && tb->type == UMALLOC);
  if (ta->max_free_area_len > tb->max_free_area_len)
    return 1;
  else if (ta->max_free_area_len < tb->max_free_area_len)
    return -1;
  else
    return 0;
}

/*
1)首先遍历vm.partial里的vma，看看max_free_area_len是不是>=size，如果是就跳到(3)
2)创建ROUND(size, 4K)这么大的vma，然后设置好free_area
3)通过vma里freearea(从小到大排序)，first-fit(因为排序了，也是best-fit)分配一个合适的虚拟地址返回，同时在内部记录此地址对应的分配长度
注意确保分配出去的内存总是对齐到MAX_ALIGNMENT
参数lazy_map指示在分配vma时，是否延迟映射实际物理页
在函数返回了非0值并有提供out_vma参数时，out_vma将被设置为指向分配内存所属vma的指针
在函数返回了非0值并有提供out_physical参数时，out_physical将被设置为分配内存的物理地址
*/
uintptr_t umalloc(struct virtual_memory *vm, uint32_t size, bool lazy_map,
                  struct virtual_memory_area **out_vma, uintptr_t *out_physical,
                  uint32_t vm_mutex) {
  if (size == 0) {
    // malloc(0) will return either "a null pointer or a unique pointer that can
    // be successfully passed to free()".
    *out_physical = 1;
    return 1;
  }
  assert(vm);
  SMART_LOCK(l, vm_mutex)
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "****umalloc(vm, %lld, %s)****\n",
               (int64_t)size, lazy_map ? "true" : "false");
#endif
  struct virtual_memory_area *vma = 0;
  // 1.首先遍历vm.partial里的vma，看看max_free_area_len是不是>=size，如果是就跳到3
  for (list_entry_t *p = list_next(&vm->partial); p != &vm->partial;
       p = list_next(p)) {
    struct virtual_memory_area *pvma = entry2vma(p);
    if (pvma->max_free_area_len >= size) {
      vma = pvma;
#ifdef VERBOSE
      printf_color(CGA_COLOR_LIGHT_YELLOW, "found useable vma in partial\n");
#endif
      break;
    }
  }
  if (vma == 0) {
    // 2.没有找到合适的vma，那么我们创建一个新的vma
    uint32_t vma_size = ROUNDUP(size, 4096);
#ifdef VERBOSE
    printf_color(CGA_COLOR_LIGHT_YELLOW, "create new MALLOC vma size %lld\n",
                 (int64_t)vma_size);
#endif
    uintptr_t vma_start =
        virtual_memory_find_fit(vm, vma_size, USER_CODE_BEGIN, USER_SPACE_END,
                                PTE_U | PTE_W | PTE_P, UMALLOC);
    if (vma_start == 0) {
      panic("unlikely...");
      return 0;
    }
    // 分配虚拟内存
    // 注意，这里merge选了false，这是为了让每个vma粒度更小，这样更容易被释放
    vma = virtual_memory_alloc(vm, vma_start, vma_size, PTE_U | PTE_W | PTE_P,
                               UMALLOC, false);
    assert(vma);
    list_init(&vma->list_node);
    list_sort_add(&vm->partial, &vma->list_node, compare_malloc_vma, 32);
    // 增加freearea
    struct umalloc_free_area *fa = new_free_area();
    fa->addr = vma_start;
    fa->len = vma_size;
    list_init(&vma->free_area_sort_by_len);
    list_init(&fa->list_head);
    list_add(&vma->free_area_sort_by_len, &fa->list_head);
    avl_tree_init(&vma->free_area_sort_by_addr, compare_free_area_by_addr,
                  sizeof(struct umalloc_free_area), 0);
    avl_node_init(&fa->avl_head);
    if (avl_tree_add(&vma->free_area_sort_by_addr, fa)) {
      panic("umalloc avl_tree_add");
    }
    vma->max_free_area_len = vma_size;
    avl_tree_init(&vma->allocated_free_area, compare_free_area_by_addr,
                  sizeof(struct umalloc_free_area), 0);
    vma->physical = 0;
  }

#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "vma.start = 0x%08llx\n",
               (int64_t)vma->start);
#endif

  /*
  3.从vma中分配一个freearea出来
  这里看起来是first-fit，但同时也是best-fit，因为vma里的freearea是从小到大排序的
  注意确保分配出去的内存总是对齐到MAX_ALIGNMENT
  */
  for (list_entry_t *li = list_next(&vma->free_area_sort_by_len);
       li != &vma->free_area_sort_by_len; li = list_next(li)) {
    struct umalloc_free_area *free_area = offset_free_area(li);
    uint32_t actual_size = ROUNDUP(size, 32);
    if (free_area->len >= actual_size) {
#ifdef VERBOSE
      printf_color(CGA_COLOR_LIGHT_YELLOW, "found freearea of size %lld\n",
                   (int64_t)free_area->len);
#endif
      // 如果这个freearea够大
      uint32_t free_area_len = free_area->len;
      uintptr_t addr = free_area->addr;
      struct umalloc_free_area *record = 0;
      assert(addr % MAX_ALIGNMENT == 0);
      list_del(&free_area->list_head);
      avl_tree_remove(&vma->free_area_sort_by_addr, &free_area->avl_head);
      if (free_area->len > actual_size) {
        // freearea分配完之后还有剩余的空间，我们修改完freearea的size后还把它重新加到list里
        free_area->len -= actual_size;
        free_area->addr += actual_size;
#ifdef VERBOSE
        printf_color(CGA_COLOR_LIGHT_YELLOW,
                     "after allocating %lld, freearea has %lld left\n",
                     (int64_t)actual_size, (int64_t)free_area->len);
#endif
        list_init(&free_area->list_head);
        list_sort_add(&vma->free_area_sort_by_len, &free_area->list_head,
                      compare_free_area_by_len, 16);
        avl_node_init(&free_area->avl_head);
        if (avl_tree_add(&vma->free_area_sort_by_addr, free_area)) {
          panic("umalloc avl_tree_add 2");
        }
        // 新建record
        record = new_free_area();
        assert(record);
      } else {
        // 整个freearea都被用掉了，freearea结构可以用来当record
        assert(free_area->len == actual_size);
        record = free_area;
#ifdef VERBOSE
        printf_color(CGA_COLOR_LIGHT_YELLOW,
                     "freearea are been totally allocated\n");
#endif
      }
      if (free_area_len == vma->max_free_area_len) {
        // 如果这次用的freearea正好是最大的那个，更新max_free_area_len
        if (list_empty(&vma->free_area_sort_by_len)) {
          vma->max_free_area_len = 0;
        } else {
          vma->max_free_area_len =
              offset_free_area(vma->free_area_sort_by_len.prev)->len;
          assert(vma->max_free_area_len != 0);
        }
#ifdef VERBOSE
        printf_color(CGA_COLOR_LIGHT_YELLOW,
                     "update max_free_area_len to %lld\n",
                     (int64_t)vma->max_free_area_len);
#endif
      }
      if (list_empty(&vma->free_area_sort_by_len)) {
        // 如果freearea用完了，把vma移动到full链表里
        assert(vma->max_free_area_len == 0);
        list_del(&vma->list_node);
        list_init(&vma->list_node);
        list_add(&vm->full, &vma->list_node);
#ifdef VERBOSE
        printf_color(CGA_COLOR_LIGHT_YELLOW, "move vma to full\n");
#endif
      }
      // 记录本次分配的地址和长度
      avl_node_init(&record->avl_head);
      record->addr = addr;
      record->len = actual_size;
      if (avl_tree_add(&vma->allocated_free_area, record)) {
        panic("umalloc avl_tree_add 3");
      }
      if (!lazy_map && vma->physical == 0) {
        upfault(vm, vma);
      }
      if (out_vma) {
        *out_vma = vma;
      }
      if (out_physical) {
        if (vma->physical) {
          *out_physical = (uintptr_t)(vma->physical + (addr - vma->start));
        } else {
          *out_physical = 0;
        }
      }
#ifdef VERBOSE
      printf_color(CGA_COLOR_LIGHT_YELLOW, "****umalloc return with 0x%08llx****\n",
                   (int64_t)addr);
#endif
      return addr;
    }
  }
  abort();
  __unreachable;
}

// malloc的vma缺页时候，把一整个vma都映射上物理内存
// 调用者需要确保已经加vm mutex锁
void upfault(struct virtual_memory *vm, struct virtual_memory_area *vma) {
  assert(vma->type == UMALLOC);
  assert(vma->size >= 4096 && vma->size % 4096 == 0);
  uintptr_t physical = free_region_page_alloc(vma->size / 4096);
  if (physical == 0) {
    // 1.swap  2.如果不能swap，让进程崩溃
    // 考虑此函数有可能是从umalloc里调用的
    abort();
  }
  virtual_memory_map(vm, vma, vma->start, vma->size, physical);
  vma->physical = physical;
  lcr3(rcr3());
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW,
               "upfault physical 0x%08llx to virtual 0x%08llx, length %lld\n",
               (int64_t)(uintptr_t)physical, (int64_t)vma->start,
               (int64_t)vma->size);
#endif
}

// 修改freearea(确保从小到大排序)，然后看如果一整个vma都是free的，那么就删除vma，释放物理内存
void ufree(struct virtual_memory *vm, uintptr_t addr, uint32_t vm_mut) {
  if (addr == 1)
    return;
  SMART_LOCK(l, vm_mut)
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "****ufree(vm, 0x%08llx)****\n",
               (int64_t)addr);
#endif
  struct virtual_memory_area *vma = virtual_memory_get_vma(vm, addr);
  assert(vma && vma->type == UMALLOC);
  const bool is_full = list_empty(&vma->free_area_sort_by_len);
  // 1.查表得到size
  struct umalloc_free_area find;
  find.addr = addr;
  struct umalloc_free_area *corresponding =
      avl_tree_find(&vma->allocated_free_area, &find);
  assert(corresponding && corresponding->addr == addr);
  // 确认addr+size也是在vma里的
  assert(corresponding->addr + corresponding->len > vma->start &&
         corresponding->addr + corresponding->len <= vma->start + vma->size);
  avl_tree_remove(&vma->allocated_free_area, corresponding);
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW,
               "vma.start = 0x%08llx, free size = %lld\n", (int64_t)vma->start,
               (int64_t)corresponding->len);
#endif
  // 2.看有没有freearea是和这区域相邻的，如果有就合并
  struct umalloc_free_area *prev = avl_tree_nearest(
                               &vma->free_area_sort_by_addr, corresponding),
                           *next = 0;
  if (prev) {
    if (prev->addr >= corresponding->addr) {
      next = prev;
      prev = avl_tree_prev(&vma->free_area_sort_by_addr, prev);
    } else {
      next = avl_tree_next(&vma->free_area_sort_by_addr, prev);
    }
  }
  assert(!prev || prev->addr < corresponding->addr);
  assert(!next || next->addr > corresponding->addr);

  if (prev && prev->addr + prev->len == corresponding->addr) {
    // 如果prev可以合并到corrsponding
    corresponding->addr = prev->addr;
    corresponding->len += prev->len;
    avl_tree_remove(&vma->free_area_sort_by_addr, prev);
    list_del(&prev->list_head);
    delete_free_area(prev);
#ifdef VERBOSE
    printf_color(CGA_COLOR_LIGHT_YELLOW, "prev -> corresponding\n");
#endif
  }
  if (next && corresponding->addr + corresponding->len == next->addr) {
    // 如果next可以合并到corrsponding
    corresponding->len += next->len;
    avl_tree_remove(&vma->free_area_sort_by_addr, next);
    list_del(&next->list_head);
    delete_free_area(next);
#ifdef VERBOSE
    printf_color(CGA_COLOR_LIGHT_YELLOW, "corresponding <- next\n");
#endif
  }
  list_init(&corresponding->list_head);
  list_sort_add(&vma->free_area_sort_by_len, &corresponding->list_head,
                compare_free_area_by_len, 16);
  avl_node_init(&corresponding->avl_head);
  if (avl_tree_add(&vma->free_area_sort_by_addr, corresponding)) {
    panic("ufree avl_tree_add");
  }
  // 更新max_free_area_len
  vma->max_free_area_len =
      offset_free_area(vma->free_area_sort_by_len.prev)->len;
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "max_free_area_len set to %lld\n",
               (int64_t)vma->max_free_area_len);

#endif
  // 3.如果一整个vma都是free的，那么直接删除整个vma，释放物理内存
  assert(!list_empty(&vma->free_area_sort_by_len));
  {
    struct umalloc_free_area *only_one =
        offset_free_area(vma->free_area_sort_by_len.next);
    if (only_one->list_head.next == &vma->free_area_sort_by_len) {
      // 如果vma->free_area只有1个元素
      if (only_one->addr == vma->start && only_one->len == vma->size) {
        // 并且这个元素描述了整个vma区域，这说明vma已经空了
        // 删除vma并释放物理内存
        assert(vma->max_free_area_len == vma->size &&
               vma->allocated_free_area.count == 0);
        virtual_memory_unmap(vm, vma->start, vma->size);
        malloc_vma_destroy(vma);
        virtual_memory_free(vm, vma);
        lcr3(rcr3());
#ifdef VERBOSE
        printf_color(CGA_COLOR_LIGHT_YELLOW, "vma deleted\n");
#endif
#ifdef VERBOSE
        printf_color(CGA_COLOR_LIGHT_YELLOW, "****ufree return****\n",
                     (int64_t)addr);
#endif
        return;
      }
    }
  }
  // 4.更新vma所在的链表(从full移动到partial)
  if (is_full) {
    // 此时vma->free_area只有1个元素
    assert(vma->free_area_sort_by_len.next &&
           vma->free_area_sort_by_len.next->next == 0);
    list_del(&vma->list_node);
    list_add(&vm->partial, &vma->list_node);
#ifdef VERBOSE
    printf_color(CGA_COLOR_LIGHT_YELLOW, "vma have been moved to partial\n");
#endif
  }
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "****ufree return****\n", (int64_t)addr);
#endif
}

// 创建共享内存，返回共享内存id
// 若返回0表示失败
uint32_t shared_memory_create(size_t size) {
  struct shared_memory *sh = malloc(sizeof(struct shared_memory));
  if (!sh)
    return 0;
  sh->pgcnt = ROUNDUP(size, _4K) / _4K;
  sh->physical = free_region_page_alloc(sh->pgcnt);
  if (!sh->physical) {
    free(sh);
    return 0;
  }
  sh->ref = 0;
  avl_node_init(&sh->head);
  sh->id = kernel_object_new(KERNEL_OBJECT_SHARED_MEMORY, sh);
  return sh->id;
}

bool shared_memory_destroy(struct shared_memory *sh) {
  // 这个检查已经在kernel_object框架里做了
  // assert(sh->ref == 0);
  free_region_page_free(sh->physical, sh->pgcnt);
  free(sh);
  return true;
}

// 获得共享内存的上下文
struct shared_memory *shared_memory_ctx(uint32_t id) {
  return kernel_object_get(id);
}

// map共享内存到当前地址空间
// 当addr为0时表示自动选择映射到的地址
// 返回映射到的虚拟地址，返回0表示错误
void *shared_memory_map(uint32_t id, void *addr) {
  SMART_LOCK(l, task_current()->group->vm_mutex)
  struct virtual_memory *vm = task_current()->group->vm;
  struct shared_memory *sh = shared_memory_ctx(id);
  assert(sh && vm);
  if (!addr) {
    // 自动寻找地址
    addr = (void *)virtual_memory_find_fit(vm, sh->pgcnt * _4K, USER_CODE_BEGIN,
                                           USER_SPACE_END,
                                           PTE_P | PTE_U | PTE_W, SHM);
    if (!addr)
      return 0;
  }
  struct virtual_memory_area *vma = virtual_memory_alloc(
      vm, (uintptr_t)addr, sh->pgcnt * _4K, PTE_P | PTE_U | PTE_W, SHM, false);
  virtual_memory_map(vm, 0, (uintptr_t)addr, sh->pgcnt * _4K, sh->physical);
  bool abs = kernel_object_ref(task_current()->group, id);
  if (!abs)
    abort();
  vma->shid = id;
  lcr3(rcr3());
  return addr;
}

void shared_memory_unmap(void *addr) {
  SMART_LOCK(l, task_current()->group->vm_mutex)
  struct virtual_memory *vm = task_current()->group->vm;
  // 为了防止这里修改vm时，别的线程被调进来运行
  SMART_CRITICAL_REGION
  struct virtual_memory_area *vma = virtual_memory_get_vma(vm, (uintptr_t)addr);
  assert(vma);
  if (vma->type != SHM)
    panic("vma->type != SHM");
  {
    SMART_CRITICAL_REGION
    struct shared_memory *sh = shared_memory_ctx(vma->shid);
    assert(sh && sh->id == vma->shid);
    kernel_object_unref(task_current()->group, sh->id, true);
  }
  virtual_memory_unmap(vm, vma->start, vma->size);
  virtual_memory_free(vm, vma);
  lcr3(rcr3());
}
