#include <assert.h>
#include <defs.h>
#include <memory_manager.h>
#include <stdio.h>
#include <tty.h>

void kmem_free_region_init(struct e820map_t *memlayout) {
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
    kmem_page_add_free_region((uintptr_t)addr, page_count * 4096 * 1024);
    printf("adding 0x%09llx length %lld to free region\n",
           (int64_t)(uintptr_t)addr, (int64_t)page_count * 4096 * 1024);
    terminal_color(CGA_COLOR_LIGHT_GREEN, CGA_COLOR_DARK_GREY);
    printf("free region ok\n");
    terminal_default_color();
  }
}
