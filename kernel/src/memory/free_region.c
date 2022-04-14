#include <assert.h>
#include <defs.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <stdio.h>
#include <sync.h>
#include <task.h>
#include <tty.h>
#include <virtual_memory.h>
#include <x86.h>

//#define VERBOSE

void free_region_init(struct e820map_t *memlayout) {
  // 开始将FREE REGION分配到kmem_page_alloc里
  for (uint32_t i = 0; i < memlayout->count; i++) {
    // BIOS保留的内存
    if (!E820_ADDR_AVAILABLE(memlayout->ard[i].type)) {
      continue;
    }
    // 高于4G的内存
    if (memlayout->ard[i].addr >= 0xffffffff) {
      continue;
    }
    // 低于normal region的内存
    if (memlayout->ard[i].addr + memlayout->ard[i].size <
        normal_region_paddr + normal_region_size) {
      continue;
    }
    // 4M对齐
    char *addr = (char *)(uintptr_t)memlayout->ard[i].addr;
    if ((uintptr_t)addr < normal_region_paddr + normal_region_size) {
      // 如果addr在normal region中或更低，那么我们让他至少从normalregion末尾开始
      assert(normal_region_paddr + normal_region_size < 0xffffffff);
      addr = (char *)(normal_region_paddr + normal_region_size);
    }
    assert(ROUNDUP((uint64_t)(uintptr_t)addr, _4M) < 0xffffffff);
    addr = (char *)ROUNDUP((uintptr_t)addr, _4M);
    assert((uint64_t)(uintptr_t)addr > memlayout->ard[i].addr);
    assert((uintptr_t)addr >= normal_region_paddr + normal_region_size);
    assert((uintptr_t)addr < 0xFFFFFFFF);
    uint32_t round_diff = (uintptr_t)addr - (uintptr_t)memlayout->ard[i].addr;
    uint32_t page_count =
        (uint32_t)((memlayout->ard[i].size - round_diff) / _4M);
    // 如果对齐之后发现凑不到1页了，那这块内存就没用了
    if (page_count == 0) {
      continue;
    }
    // 对于addr+size越过4G界的处理
    if ((uint64_t)(uintptr_t)addr + (uint64_t)(page_count * _4M) >=
        0xFFFFFFFF) {
      assert((0xFFFFFFFF - (uint64_t)(uintptr_t)addr) % _4M == 0);
      page_count = (0xFFFFFFFF - (uint64_t)(uintptr_t)addr) / _4M;
    }
    // 加入kmem_page
    kmem_page_init_free_region((uintptr_t)addr, page_count * 4096 * 1024);
#ifdef VERBOSE
    printf("adding 0x%09llx length %lld to free region\n",
           (int64_t)(uintptr_t)addr, (int64_t)page_count * 4096 * 1024);
#endif
    terminal_color(CGA_COLOR_LIGHT_GREEN, CGA_COLOR_DARK_GREY);
    printf("free region ok\n");
    terminal_default_color();
  }
}

uintptr_t free_region_page_alloc(size_t cnt) {
  return (uintptr_t)alloc_page_impl(FREE_REGION, cnt);
}

void free_region_page_free(uintptr_t addr, size_t cnt) {
  free_page_impl(FREE_REGION, addr, cnt);
}

void *free_region_access(uintptr_t physical, size_t length) {
  SMART_CRITICAL_REGION
  struct virtual_memory *current_vm = virtual_memory_current();
  uintptr_t vaddr = virtual_memory_find_fit(current_vm, ROUNDUP(length, _4K),
                                            map_region_vaddr, map_region_vend,
                                            PTE_P | PTE_W, KMAP);
  assert(vaddr == ROUNDDOWN(vaddr, _4K));
  if (!vaddr)
    return 0;
  struct virtual_memory_area *vma = virtual_memory_alloc(
      current_vm, vaddr, ROUNDUP(length, _4K), PTE_P | PTE_W, KMAP, false);
  if (!vma)
    return 0;
  virtual_memory_map(current_vm, vma, vaddr, ROUNDUP(length, _4K),
                     ROUNDDOWN(physical, _4K));
  lcr3(rcr3());
  uintptr_t round_diff = physical - ROUNDDOWN(physical, _4K);
  return (void *)vaddr + round_diff;
}

void free_region_no_access(void *virtual) {
  SMART_CRITICAL_REGION
  struct virtual_memory *current_vm = virtual_memory_current();
  struct virtual_memory_area *vma =
      virtual_memory_get_vma(current_vm, (uintptr_t) virtual);
  assert(vma && vma->type == KMAP);
  virtual_memory_unmap(current_vm, vma->start, vma->size);
  lcr3(rcr3());
  virtual_memory_free(current_vm, vma);
}
