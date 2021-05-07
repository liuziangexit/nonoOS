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
TODO 合并邻近的VMA，小页表全部存的是物理地址，支持大小页混合的情况
-合并VMA
-合并页表为4M大页
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

//从一个已有的页目录里建立vma
void virtual_memory_clone(struct virtual_memory *vm,
                          const uint32_t *page_directory) {
  assert(((uintptr_t)page_directory) % _4K == 0);
  for (uint32_t pd_idx = 0; pd_idx < 1024; pd_idx++) {
    if (page_directory[pd_idx] & PTE_P) {
      //对于每个PDE
      if (page_directory[pd_idx] & PTE_PS) {
        // 4M页
        uintptr_t ps_page_frame =
            (uintptr_t)(page_directory[pd_idx] & 0xFFC00000);
        bool ret = virtual_memory_map(vm, pd_idx * _4M, _4M, ps_page_frame,
                                      page_directory[pd_idx] & 7);
        assert(ret);
      } else {
        // 4K页
        uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & ~0xFFF);
        for (uint32_t pt_idx = 0; pt_idx < 1024; pt_idx++) {
          if (pt[pt_idx] & PTE_P) {
            uintptr_t page_frame = (uintptr_t)(pt[pt_idx] & ~0xFFF);
            bool ret = virtual_memory_map(vm, pt_idx * _4K + pd_idx * _4M, _4K,
                                          page_frame, pt[pt_idx] & 7);
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
      kmem_page_free(P2V(page_table), 1);
    }
  }
  kmem_page_free(vm->page_directory, 1);
  //遍历二叉树，释放掉节点
  //本来我还以为要自己写后序遍历，没想到云老师已经做了这个需求，祝他长命百岁
  avl_tree_clear(&vm->vma_tree, free);
  //最后释放vm结构
  free(vm);
}

//寻找对应的vma，如果没有返回0
struct virtual_memory_area *virtual_memory_get_vma(struct virtual_memory *vm,
                                                   uint32_t vma_start) {
  assert(vma_start % 4096 == 0);
  struct virtual_memory_area vma;
  vma.vma_start = vma_start;
  return (struct virtual_memory_area *)avl_tree_find(&vm->vma_tree, &vma);
}

//在一个虚拟地址空间结构中进行以4k为边界映射
//返回false如果指定的虚拟地址已经有映射了，或者系统没有足够的内存
bool virtual_memory_map(struct virtual_memory *vm, uintptr_t vma_start,
                        uintptr_t vma_size, uintptr_t physical_addr,
                        uint16_t flags) {
  assert(vma_start % 4096 == 0 && vma_size % 4096 == 0 &&
         physical_addr % 4096 == 0);
  //我们只允许US、RW和P
  assert(flags >> 3 == 0);
  //确定一下有没有重叠
  assert(vma_start == virtual_memory_find_fit(vm, vma_size, vma_start,
                                              vma_start + vma_size));
  //确认没有重叠，开始新增了
  struct virtual_memory_area *vma = malloc(sizeof(struct virtual_memory_area));
  if (!vma) {
    return false;
  }
  vma->vma_start = vma_start;
  vma->vma_size = vma_size;
  if (avl_tree_add(&vm->vma_tree, vma)) {
    // never!
    abort();
  }
  //逐个在页目录里map
  //在这个函数中，PTE和PDE的这些flags必须是同样的，也就是说参数flags不仅
  //是新的PTE的flags，也必须和已有的PDE flags不冲突
  for (uint32_t p = vma_start; p < vma_start + vma_size;
       p += _4K, physical_addr += _4K) {
    uint32_t pd_idx = p / _4M, pt_idx = 0x3FF & (p >> 12);
    uint32_t *pde = &vm->page_directory[pd_idx];
    if (((*pde) & PTE_P) == 0) {
      // PDE是空的，分配一个页表
      uint32_t *pt = kmem_page_alloc(1);
      // FIXME 这里失败的时候，应该要回收本次函数调用已分配的结构，并且返回false
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

void virtual_memory_unmap(struct virtual_memory *vm, uintptr_t vma_start) {
  assert(vma_start % 4096 == 0);
  struct virtual_memory_area key;
  key.vma_start = vma_start;
  struct virtual_memory_area *vma = avl_tree_find(&vm->vma_tree, &key);
  assert(vma); //用户参数有问题，直接崩
  //在页表里取消这些地址的map
  for (uintptr_t p = vma->vma_start; p < vma->vma_start + vma->vma_size;
       p += _4K) {
    uint32_t pd_idx = p / _4M, pt_idx = 0x3FF & (p >> 12);
    assert(vm->page_directory[pd_idx] & PTE_P);
    assert((vm->page_directory[pd_idx] & PTE_PS) == 0);
    uint32_t *pt = (uint32_t *)(vm->page_directory[pd_idx] & ~0xFFF);
    pt[pt_idx] = 0;
  }
  avl_tree_remove(&vm->vma_tree, vma);
  free(vma);
}

//在一个虚拟地址空间结构中寻找[begin,end)中空闲的指定长度的地址空间
//返回0表示找不到
uintptr_t virtual_memory_find_fit(struct virtual_memory *vm, uint32_t vma_size,
                                  uintptr_t begin, uintptr_t end) {
  assert(end - begin >= _4K && vma_size % _4K == 0);
  uintptr_t real_begin = ROUNDUP(begin, _4K), real_end = ROUNDUP(end, _4K);
  assert(real_end - real_begin >= vma_size);
  struct virtual_memory_area key;
  key.vma_start = real_begin;
  struct virtual_memory_area *nearest = avl_tree_nearest(&vm->vma_tree, &key);
  if (nearest) {
    //确定空闲内存起始
    if (nearest->vma_start <= real_begin) {
      if (nearest->vma_start + nearest->vma_size > real_begin) {
        real_begin = nearest->vma_start + nearest->vma_size;
        if (real_end - real_begin < vma_size) {
          //新映射的空间至少前半部分被占用了，并且剩下的部分不够大了
          return 0;
        }
      }
      nearest = avl_tree_next(&vm->vma_tree, nearest);
      if (nearest->vma_start < real_begin) {
        //说明没有比nearest大的key了
        nearest = 0;
      }
    }
    if (nearest) {
      assert(nearest->vma_start >= real_begin);
      if (nearest->vma_start == real_begin) {
        //这种情况只有可能是从上面的nearest->vma_start <= real_begin里面出来的
        //这意味着新映射的空间完全被占用满了
        return 0;
      }
      if (nearest->vma_start > real_begin) {
        //现在我们来看后半部分有没有被占用，如果有，剩下的空间够不够大
        if (nearest->vma_start < real_end) {
          //后半部分被占用了
          real_end = nearest->vma_start;
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

bool virtual_memory_free(struct virtual_memory *vm, void *p) {
  uintptr_t vma_start = (uintptr_t)p;
  uint32_t pd_idx = vma_start / _4M, pt_idx = 0x3FF & (vma_start >> 12);
  if ((vm->page_directory[pd_idx] & PTE_P) == 0) {
    return false;
  }
  uint32_t *pt = (uint32_t *)(vm->page_directory[pd_idx] & ~0xFFF);
  if ((pt[pt_idx] & PTE_P) == 0) {
    return false;
  }
  void *ptr = (void *)(pt[pt_idx] & ~0xFFF);
  if (!kmem_free(P2V(ptr))) {
    return false;
  }
  virtual_memory_unmap(vm, vma_start);
  return true;
}
