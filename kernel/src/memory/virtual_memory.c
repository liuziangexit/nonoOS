#include <avlmini.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <panic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <virtual_memory.h>

/*
本文件主要包含用户进程虚拟地址空间管理
TODO 合并邻近的VMA，小页表全部存的是物理地址，支持大小页混合的情况
-合并VMA
-合并页表为4M大页
*/

int vma_compare(const void *a, const void *b) {
  const struct virtual_memory_area *ta = (const struct virtual_memory_area *)a;
  const struct virtual_memory_area *tb = (const struct virtual_memory_area *)b;
  return ta->start - tb->start;
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
            vm, pd_idx * _4M, _4M, page_directory[pd_idx] & 7, type);
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
            struct virtual_memory_area *vma = virtual_memory_alloc(
                vm, pt_idx * _4K + pd_idx * _4M, _4K, pt[pt_idx] & 7, type);
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

//在一个虚拟地址空间结构中寻找[begin,end)中空闲的指定长度的地址空间
//返回0表示找不到
uintptr_t virtual_memory_find_fit(struct virtual_memory *vm, uint32_t vma_size,
                                  uintptr_t begin, uintptr_t end) {
  assert(end - begin >= vma_size && vma_size >= _4K && vma_size % _4K == 0);
  uintptr_t real_begin = ROUNDUP(begin, _4K), real_end = ROUNDUP(end, _4K);
  assert(real_end - real_begin >= vma_size);
  struct virtual_memory_area key;
  key.start = real_begin;
  struct virtual_memory_area *nearest = avl_tree_nearest(&vm->vma_tree, &key);
  if (nearest) {
    //确定空闲内存起始
    if (nearest->start <= real_begin) {
      if (nearest->start + nearest->size > real_begin) {
        real_begin = nearest->start + nearest->size;
        if (real_end - real_begin < vma_size) {
          //新映射的空间至少前半部分被占用了，并且剩下的部分不够大了
          return 0;
        }
      }
      nearest = avl_tree_next(&vm->vma_tree, nearest);
      if (nearest == 0 || nearest->start < real_begin) {
        //说明没有比nearest大的key了
        nearest = 0;
      }
    }
    if (nearest) {
      assert(nearest->start >= real_begin);
      if (nearest->start == real_begin) {
        //这种情况只有可能是从上面的nearest->start <= real_begin里面出来的
        //这意味着新映射的空间完全被占用满了
        return 0;
      }
      if (nearest->start > real_begin) {
        //现在我们来看后半部分有没有被占用，如果有，剩下的空间够不够大
        if (nearest->start < real_end) {
          //后半部分被占用了
          real_end = nearest->start;
        }
        //看看够不够大呢
        if (real_end - real_begin < vma_size) {
          //不够大
          return 0;
        }
        //够大
      }
    }
  }
  //不太放心，再确认下
  assert(real_begin % 4096 == 0 && real_end - real_begin >= vma_size);
  return real_begin;
}

static struct umalloc_free_area *new_free_area() {
  struct umalloc_free_area *p = malloc(sizeof(struct umalloc_free_area));
  memset(p, 0, sizeof(struct umalloc_free_area));
  return p;
}

// static void delete_free_area(struct umalloc_free_area *ufa) { free(ufa); }

struct virtual_memory_area *
virtual_memory_alloc(struct virtual_memory *vm, uintptr_t vma_start,
                     uintptr_t vma_size, uint16_t flags,
                     enum virtual_memory_area_type type) {
  //我们只允许US、RW和P
  assert(flags >> 3 == 0);
  //确定一下有没有重叠
  if (vma_start !=
      virtual_memory_find_fit(vm, vma_size, vma_start, vma_start + vma_size)) {
    panic("vma_start != virtual_memory_find_fit");
  }
  //确认没有重叠，开始新增了
  //首先获得vma_start之前最近的vma，叫他prev
  struct virtual_memory_area *prev = 0;
  {
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
  if (
      // 如果有前一个vma
      prev
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
    vma->start = vma_start;
    vma->size = vma_size;
    vma->flags = flags;
    vma->type = type;
    if (type == MALLOC) {
      //注意！此时既不在partial也不在full
      list_init(&vma->list_node);
      //增加freearea
      struct umalloc_free_area *fa = new_free_area();
      fa->addr = vma_start;
      fa->len = vma_size;
      list_add(&vma->free_area, (list_entry_t *)fa);
      vma->max_free_area_len = vma_size;
    }
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

// uintptr_t umalloc(uint32_t alignment, uint32_t size) {}

// void umalloc_pgfault(struct virtual_memory_area *vma, uintptr_t addr) {}

// void ufree(uintptr_t addr) {}