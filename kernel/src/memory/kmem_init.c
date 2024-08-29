#include <assert.h>
#include <cga.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <panic.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>
#include <x86.h>

/*
物理内存上看，最开始16MB是VMA区域，其中较低部分包含了CGA BUFFER等硬件映射的内存
接着是kernel代码从16MB开始
kernel代码后的第一个4M地址上是bootstack，长度是4MB
bootstack后第一个4M地址上是NORMAL REGION，长度不大于total memory/4
剩下的物理内存都是FREE REGION，也就是umalloc用的

从虚拟内存(这里指的是内核的虚拟内存)上看，最初0-0xC0000000(3GB)没有映射，保留给用户态
3GB开始，第一个16MB是VMA区域的直接映射
接着从3G+16M开始，是内核代码直接映射
接着是NORMAL REGION直接映射
以上的直接映射是指物理地址=虚拟地址-3GB
最后3GB+896MB(KERNEL_MAP_REGION)开始一直到虚拟地址末尾是MAP区域

注意，下面说的页基本上是指4M页
*/
void kmem_init(struct e820map_t *memlayout) {
  extern uint32_t kernel_pd[];
  _Alignas(_4K) uint32_t temp_pd[1024];
  memcpy(temp_pd, kernel_pd, _4K);
  lcr3(boot_stack_v2p((uintptr_t)temp_pd));
  /*
  现在已经有
  1)DMA_REGION
  2)程序映像的映射，从16MB到3G+16MB
  3)boot stack的映射
  接下来加上NORMAL_REGION的映射
  */
  // 确定normal_region的虚拟地址
  normal_region_vaddr = KERNEL_VIRTUAL_BASE + boot_stack_paddr + _4M;
  assert(normal_region_vaddr % _4M == 0);
  // 这个for只是为了计算系统内存总量
  uint32_t total_memory = 0;
  for (uint32_t i = 0; i < memlayout->count; i++) {
    // BIOS保留的内存
    if (!E820_ADDR_AVAILABLE(memlayout->ard[i].type)) {
      continue;
    }
    // 高于4G
    if (memlayout->ard[i].addr >= 0xffffffff) {
      continue;
    }
    if (memlayout->ard[i].addr + memlayout->ard[i].size > 0xffffffff) {
      // 如果尾端超出4G，那么截去超出部分
      total_memory += (uint32_t)((uint64_t)0xffffffff - memlayout->ard[i].addr);
    } else {
      total_memory += memlayout->ard[i].size;
    }
  }
  // 现在计算normal_region的大小
  normal_region_size = total_memory / 4;
  // 1G-4M(boot stack用的)-normal_region=还剩多少虚拟地址空间
  uint32_t vm_left = 0xffffffff - 0x400000 - normal_region_vaddr;
  if (normal_region_size > vm_left) {
    normal_region_size = vm_left;
  }
  normal_region_size = ROUNDDOWN(normal_region_size, _4M);
  assert(normal_region_size >= _4M);
  printf("kmem_init: total_memory       = %lld\n", (int64_t)total_memory);
  printf("kmem_init: normal_region_vaddr= 0x%09llx\n",
         (int64_t)normal_region_vaddr);
  printf("kmem_init: normal_region_size = %lld\n", (int64_t)normal_region_size);

  // 开始确定normal_region的物理地址
  normal_region_paddr = 0;
  for (uint32_t i = 0; i < memlayout->count; i++) {
    // BIOS保留的内存
    if (!E820_ADDR_AVAILABLE(memlayout->ard[i].type)) {
      continue;
    }
    // 小于normal_region_size的内存
    if (memlayout->ard[i].size < normal_region_size) {
      continue;
    }
    // 高于4G的内存
    if (memlayout->ard[i].addr >= 0xffffffff) {
      continue;
    }

    // 验证4M对齐后，低于4G部分的size是否足够normal_region_size
    {
      uint64_t addr = ROUNDUP(memlayout->ard[i].addr, _4M);
      if (addr >= 0xFFFFFFFF) {
        continue;
      }
      if (0xFFFFFFFF - addr < (uint64_t)normal_region_size) {
        // 4M对齐后，低于4G部分的size不够normal_region_size
        continue;
      }
    }
    // 4M对齐
    char *addr = (char *)ROUNDUP((uintptr_t)memlayout->ard[i].addr, _4M);
    uint32_t round_diff = (uintptr_t)addr - (uintptr_t)memlayout->ard[i].addr;
    uint32_t page_count =
        (uint32_t)((memlayout->ard[i].size - round_diff) / _4M);
    // 如果对齐之后发现凑不到要求的页数了，那这块内存就没用了
    if (page_count < normal_region_size / _4M) {
      continue;
    }
    // 如果addr小于NORMAL_REGION开头，就让他从NORMALREGION开始
    if ((uintptr_t)addr < boot_stack_paddr + _4M) {
      if ((uint32_t)addr + (uint32_t)(page_count * _4M) >
          (uint32_t)boot_stack_paddr + _4M) {
        page_count -= ((boot_stack_paddr + _4M - (uintptr_t)addr) / _4M);
        addr = (char *)boot_stack_paddr + _4M;
      } else {
        continue;
      }
    }
    // 如果addr大于等于4G，就忽略
    assert((uintptr_t)addr < 0xFFFFFFFF);
    // 对于addr+size越过4G界的处理
    assert((uint64_t)(uintptr_t)addr + (uint64_t)(page_count * _4M) <
           0xFFFFFFFF);
    // 对于虚拟地址越过4G界的处理
    // 虚拟地址最后4M是boot栈，所以不能改那里
    if ((uint64_t)normal_region_vaddr + ((uint64_t)page_count * _4M) >=
        (uint64_t)(0xFFFFFFFF - _4M)) {
      page_count = ((0xFFFFFFFF - _4M) - (int32_t)normal_region_vaddr) / _4M;
    }
    if (page_count < normal_region_size / _4M) {
      continue;
    }
    normal_region_paddr = (uintptr_t)addr;
    assert(normal_region_paddr == V2P(normal_region_vaddr));
    // 好了，现在准备妥当了，开始做map
    assert(normal_region_size % _4M == 0 &&
           normal_region_size / _4M <= page_count);
    pd_map(kernel_pd, normal_region_vaddr, normal_region_paddr,
           normal_region_size / _4M, PTE_P | PTE_W | PTE_PS);
    break;
  }

  if (!normal_region_paddr) {
    panic("normal_region_paddr == 0");
  }
  printf("kmem_init: normal_region_paddr= 0x%09llx\n",
         (int64_t)normal_region_paddr);

  lcr3(V2P((uintptr_t)kernel_pd));

  // 验证是不是都可以访问
  printf("checking memory: 0%%...");
  uintptr_t cnt =
      normal_region_vaddr + normal_region_size - normal_region_vaddr;
  for (uintptr_t p = normal_region_vaddr;
       p < normal_region_vaddr + normal_region_size; p += 4) {
    *(volatile uint32_t *)p = 0xFFFFFFFF;
    if (cnt != 0 && cnt / (normal_region_vaddr + normal_region_size - p) == 2) {
      printf("25%%...");
      cnt = 0;
    }
  }
  printf("50%%...");
  cnt = normal_region_vaddr + normal_region_size - normal_region_vaddr;
  for (uintptr_t p = normal_region_vaddr;
       p < normal_region_vaddr + normal_region_size; p += 4) {
    if (*(volatile uint32_t *)p != 0xFFFFFFFF) {
      panic("normal_region write test failed");
    }
    if (cnt != 0 && cnt / (normal_region_vaddr + normal_region_size - p) == 2) {
      printf("75%%...");
      cnt = 0;
    }
  }
  printf("100%%\n");
  terminal_color(CGA_COLOR_LIGHT_GREEN, CGA_COLOR_DARK_GREY);
  printf("normal_region write test passed\n");
  terminal_default_color();
  // 设置MAP REGION
  // map区最后一个字节为什么不是0xffffffff而是0xfffff000呢？
  // 因为很多地方需要一个"end"就是C++iterator语义的那种end()，如果你的最后一个字节是0xffffffff
  // 那么你的end势必成了0xffffffff+1，而这就溢出成0了
  map_region_vaddr = ROUNDUP(normal_region_vaddr + normal_region_size, _4M);
  map_region_size = ROUNDDOWN(0xffffffff - map_region_vaddr, _4K);
  map_region_vend = map_region_vaddr + map_region_size;
}
