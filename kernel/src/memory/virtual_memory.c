#include <avlmini.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <panic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <virtual_memory.h>
#include <x86.h>

/*
本文件主要包含用户进程虚拟地址空间管理
TODO 合并邻近的VMA，小页表全部存的是物理地址，支持大小页混合的情况
-合并VMA
-合并页表为4M大页
*/

int vma_compare(const void *a, const void *b) {
  const struct virtual_memory_area *ta = (const struct virtual_memory_area *)a;
  const struct virtual_memory_area *tb = (const struct virtual_memory_area *)b;
  if (ta->start > tb->start)
    return 1;
  if (ta->start < tb->start)
    return -1;
  if (ta->start == tb->start)
    return 0;
  __builtin_unreachable();
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
  list_init(&vm->full);
  list_init(&vm->partial);
  return vm;
}

//从一个已有的页目录里建立vma
void virtual_memory_clone(struct virtual_memory *vm,
                          const uint32_t *page_directory,
                          enum virtual_memory_area_type type) {
  assert(((uintptr_t)page_directory) % _4K == 0);
  for (uint32_t pd_idx = 0; pd_idx < 1024; pd_idx++) {
    if (page_directory[pd_idx] & PTE_P) {
      //对于每个PDE
      if (page_directory[pd_idx] & PTE_PS) {
        // 4M页
        uintptr_t ps_page_frame =
            (uintptr_t)(page_directory[pd_idx] & 0xFFC00000);
        struct virtual_memory_area *vma = virtual_memory_alloc(
            vm, pd_idx * _4M, _4M, page_directory[pd_idx] & 7, type, true);
        assert(vma);
        bool ret =
            virtual_memory_map(vm, vma, pd_idx * _4M, _4M, ps_page_frame);
        assert(ret);
      } else {
        // 4K页
        uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & ~0xFFF);
        for (uint32_t pt_idx = 0; pt_idx < 1024; pt_idx++) {
          if (pt[pt_idx] & PTE_P) {
            uintptr_t page_frame = (uintptr_t)(pt[pt_idx] & ~0xFFF);
            struct virtual_memory_area *vma =
                virtual_memory_alloc(vm, pt_idx * _4K + pd_idx * _4M, _4K,
                                     pt[pt_idx] & 7, type, true);
            assert(vma);
            bool ret = virtual_memory_map(vm, vma, pt_idx * _4K + pd_idx * _4M,
                                          _4K, page_frame);
            assert(ret);
          }
        }
      }
    }
  }
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
      uintptr_t page_table = entry & ~(uint32_t)0xFFF;
      kmem_page_free((void *)P2V(page_table), 1);
    }
  }
  kmem_page_free(vm->page_directory, 1);
  //遍历二叉树，释放掉节点
  //本来我还以为要自己写后序遍历，没想到云老师已经做了这个需求，祝他长命百岁
  avl_tree_clear(&vm->vma_tree, free);
  //最后释放vm结构
  free(vm);
}

// 寻找对应的vma，如果没有返回0
// TODO 此函数未测试
struct virtual_memory_area *virtual_memory_get_vma(struct virtual_memory *vm,
                                                   uint32_t mem) {
  struct virtual_memory_area vma;
  vma.start = mem;
  struct virtual_memory_area *nearest =
      (struct virtual_memory_area *)avl_tree_nearest(&vm->vma_tree, &vma);
  if (nearest->start <= mem) {
    //如果最近的在之前，那么可以直接判断mem在不在其中
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
static bool vm_same_4mpage(uintptr_t boundary,
                           struct virtual_memory_area *vma) {
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
                               uint16_t flags) {
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
      if (!vm_compare_flags(flags, vma->flags)) {
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
    __builtin_unreachable();
  }
  return real_begin;
}

// 在一个虚拟地址空间结构中寻找[begin,end)中空闲的指定长度的地址空间
// 其实就是first-fit
// 返回0表示找不到
// 这是对齐到4K的
// FIXME 这函数有问题，需要重写
uintptr_t virtual_memory_find_fit(struct virtual_memory *vm, uint32_t vma_size,
                                  uintptr_t begin, uintptr_t end,
                                  uint16_t flags) {
  assert(end > begin && vma_size >= _4K && vma_size % _4K == 0);
  uint64_t real_begin = ROUNDUP(begin, _4K), real_end = ROUNDDOWN(end, _4K);
  if (real_end - real_begin < vma_size) {
    return 0;
  }
  struct virtual_memory_area key;
  key.start = real_begin;
  struct virtual_memory_area *next = avl_tree_nearest(&vm->vma_tree, &key);
  if (next && next->start < real_begin) {
    // next是前一个
    if (next->start + next->size > real_begin) {
      //如果前一个覆盖了realbegin的位置，那么要把real_begin向后移到它的结尾
      real_begin = ROUNDUP(next->start + next->size, 4096);
    }
    next = avl_tree_next(&vm->vma_tree, next);
  }
  while (true) {
    assert(!next || next->start >= real_begin);
    if (next && next->start <= real_end) {
      // 现在我们召开民主大会，请大家给我们的候选人投票
      // 坏消息是，本次只有1名候选人参加选举；好消息是，你可以投反对票
      uint64_t end_candidate = ROUNDDOWN(next->start, 4096);
      if (end_candidate >= real_begin &&
          end_candidate - real_begin >= vma_size) {
        // 这个空闲区间满足长度要求，现在检测flags
        real_begin = vm_verify_area_flags(vm, real_begin, end_candidate,
                                          vma_size, flags);
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
        continue;
      }
    } else {
      // 如果没有next了，real_begin到real_end之间就可以用
      // 当然，有可能这空间不够大，函数的最后我们会检测这情况的
      // 检查flags先
      real_begin =
          vm_verify_area_flags(vm, real_begin, real_end, vma_size, flags);
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
  // 我们只允许US、RW和P
  assert(flags >> 3 == 0);
  // 确定一下有没有重叠
  if (vma_start != virtual_memory_find_fit(vm, vma_size, vma_start,
                                           vma_start + vma_size, flags)) {
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
    assert(prev->type != MALLOC);
    // 变大小
    prev->size += vma_size;
    return prev;
  } else {
    //新增一个vma
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
    return vma;
  }
  abort();
  __builtin_unreachable();
}

void virtual_memory_free(struct virtual_memory *vm,
                         struct virtual_memory_area *vma) {
  assert(vma);
#ifndef NDEBUG
  // debug时候确认下vma在vm里面
  struct virtual_memory_area *verify =
      (struct virtual_memory_area *)avl_tree_find(&vm->vma_tree, vma);
  assert(verify == vma);
#endif
  if (vma->type == MALLOC) {
    // 删除对应的物理内存也是umalloc_free做
    // 移除list_node的职责是umalloc_free来做，并且在做完之后就要list_init来标记
    if (!list_empty(&vma->list_node)) {
      abort();
    }
    // 销毁free_area
    // TODO遍历free_area链表，然后每个都delete_free_area（这函数现在还没有）
  }
  //从vma树中移除
  avl_tree_remove(&vm->vma_tree, vma);
  free(vma);
}

bool virtual_memory_map(struct virtual_memory *vm,
                        struct virtual_memory_area *vma, uintptr_t virtual_addr,
                        uint32_t size, uintptr_t physical_addr) {
  assert(virtual_addr % 4096 == 0 && size % 4096 == 0 &&
         physical_addr % 4096 == 0);

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
    //确定页表没问题了，现在开始改页表
    uint32_t *pt = (uint32_t *)P2V((((uint32_t)*pde) & ~0xFFF));
    union {
      struct PTE pte;
      uint32_t value;
    } punning;
    punning.value = 0;
    pte_map(&punning.pte, physical_addr, flags);
    pt[pt_idx] = punning.value;
  }
  return true;
}

void virtual_memory_unmap(struct virtual_memory *vm, uintptr_t virtual_addr,
                          uint32_t size) {
  assert(virtual_addr % 4096 == 0);
  assert(size % 4096 == 0);
  //在页表里取消这些地址的map
  for (uintptr_t p = virtual_addr; p < virtual_addr + size; p += _4K) {
    uint32_t pd_idx = p / _4M, pt_idx = 0x3FF & (p >> 12);
    assert(vm->page_directory[pd_idx] & PTE_P);
    assert((vm->page_directory[pd_idx] & PTE_PS) == 0);
    uint32_t *pt = (uint32_t *)(vm->page_directory[pd_idx] & ~0xFFF);
    pt[pt_idx] = 0;
  }
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

static struct umalloc_free_area *new_free_area() {
  struct umalloc_free_area *p = malloc(sizeof(struct umalloc_free_area));
  memset(p, 0, sizeof(struct umalloc_free_area));
  return p;
}

static void delete_free_area(struct umalloc_free_area *ufa) { free(ufa); }

static int compare_free_area(const list_entry_t *a, const list_entry_t *b) {
  const struct umalloc_free_area *ta = (const struct umalloc_free_area *)a;
  const struct umalloc_free_area *tb = (const struct umalloc_free_area *)b;
  if (ta->len > tb->len)
    return 1;
  else if (ta->len < tb->len)
    return -1;
  else
    return 0;
}

static int compare_malloc_vma(const list_entry_t *a, const list_entry_t *b) {
  const struct virtual_memory_area *ta = ((void *)a) - 32;
  const struct virtual_memory_area *tb = ((void *)b) - 32;
  assert(ta->type == MALLOC && tb->type == MALLOC);
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
*/
uintptr_t umalloc(struct virtual_memory *vm, uint32_t size) {
  struct virtual_memory_area *vma = 0;
  // 1.首先遍历vm.partial里的vma，看看max_free_area_len是不是>=size，如果是就跳到3
  for (list_entry_t *p = list_next(&vm->partial); p != &vm->partial;
       p = list_next(p)) {
    struct virtual_memory_area *pvma = entry2vma(p);
    if (pvma->max_free_area_len >= size) {
      vma = pvma;
      break;
    }
  }
  if (vma == 0) {
    // 2.没有找到合适的vma，那么我们创建一个新的vma
    uint32_t vma_size = ROUNDUP(size, 4096);
    uintptr_t vma_start = virtual_memory_find_fit(
        vm, vma_size, USER_CODE_BEGIN, USER_STACK_BEGIN, PTE_U | PTE_W | PTE_P);
    if (vma_start == 0) {
      panic("unlikely...");
      return 0;
    }
    // 分配虚拟内存
    // 注意，这里merge选了false，这是为了让每个vma粒度更小，这样更容易被释放
    vma = virtual_memory_alloc(vm, vma_start, vma_size, PTE_U | PTE_W | PTE_P,
                               MALLOC, false);
    assert(vma);
    list_init(&vma->list_node);
    list_sort_add(&vm->partial, &vma->list_node, compare_malloc_vma);
    //增加freearea
    struct umalloc_free_area *fa = new_free_area();
    fa->addr = vma_start;
    fa->len = vma_size;
    list_init(&vma->free_area);
    list_add(&vma->free_area, (list_entry_t *)fa);
    vma->max_free_area_len = vma_size;
  }

  /*
  3.从vma中分配一个freearea出来
  这里看起来是first-fit，但同时也是best-fit，因为vma里的freearea是从小到大排序的
  注意确保分配出去的内存总是对齐到MAX_ALIGNMENT
  */
  for (list_entry_t *p = list_next(&vma->free_area); p != &vma->free_area;
       p = list_next(p)) {
    struct umalloc_free_area *free_area = (struct umalloc_free_area *)p;
    uint32_t actual_size = ROUNDUP(size, 32);
    if (free_area->len >= actual_size) {
      //如果这个freearea够大
      uint32_t free_area_len = free_area->len;
      uintptr_t addr = free_area->addr;
      assert(addr % MAX_ALIGNMENT == 0);
      list_del(p);
      if (free_area->len > actual_size) {
        // freearea分配完之后还有剩余的空间，我们修改完freearea的size后还把它重新加到list里
        free_area->len -= actual_size;
        free_area->addr += actual_size;
        list_init(p);
        list_sort_add(&vma->free_area, p, compare_free_area);
      } else {
        // free_area->len == actual_size
        // 整个freearea都被用掉了，直接删除freearea结构
        delete_free_area(free_area);
      }
      if (free_area_len == vma->max_free_area_len) {
        // 如果这次用的freearea正好是最大的那个，更新max_free_area_len
        if (list_empty(&vma->free_area)) {
          vma->max_free_area_len = 0;
        } else {
          vma->max_free_area_len =
              ((struct umalloc_free_area *)vma->free_area.prev)->len;
          assert(vma->max_free_area_len != 0);
        }
      }
      if (list_empty(&vma->free_area)) {
        // 如果freearea用完了，把vma移动到full链表里
        assert(vma->max_free_area_len == 0);
        list_del(&vma->list_node);
        list_init(&vma->list_node);
        list_add(&vm->full, &vma->list_node);
      }
      // TODO 这里要用某种方式记录addr对应的size
      return addr;
    }
  }
  abort();
  __builtin_unreachable();
}

// malloc的vma缺页时候，把一整个vma都映射上物理内存
void umalloc_pgfault(struct virtual_memory *vm,
                     struct virtual_memory_area *vma) {
  assert(vma->type == MALLOC);
  assert(vma->size >= 4096 && vma->size % 4096 == 0);
  void *physical = kmem_page_alloc(vma->size / 4096);
  if (physical == 0) {
    // 1.swap  2.如果不能swap，让进程崩溃
    abort();
  }
  bool ret = virtual_memory_map(vm, vma, vma->start, vma->size,
                                V2P((uintptr_t)physical));
  if (!ret) {
    abort();
  }
  // lcr3(rcr3());
}

// 修改freearea(确保从小到大排序)，然后看如果一整个vma都是free的，那么就删除vma，释放物理内存
void ufree(struct virtual_memory *vm, uintptr_t addr) {
  // 1.查表得到size
  // 2.找到对应的vma，并确认addr+size也是在这vma里的
  // 3.遍历vma里的freearea，看有没有freearea是和这区域相邻的，如果有，直接改那freearea，否则sort
  // add进去一个新的freearea
  // 4.如果一整个vma都是free的，那么直接删除整个vma，释放物理内存
  // 5.更新max_free_area_len和vma所在的链表(从full移动到partial)
}
