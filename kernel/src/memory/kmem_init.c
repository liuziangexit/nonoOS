#include <assert.h>
#include <cga.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>
#include <x86.h>

// DMA_REGION是boot stack之后的16MB内存
// NORMAL_REGION是DMA_REGION之后的剩余内存中的1/4
// 例：总共有255MB内存，最初4MB是程序映像，第二个4MB是boot
// stack，接着是16MB的DMA_REGION，
// 然后是(255-4-4-16)/4=57MB的NORMAL_REGION，剩下的区域是FREESPACE，用于VMA
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
  normal_region = boot_stack_paddr + _4M;
  //这个for只是为了计算系统内存总量
  uint32_t total_memory;
  for (uint32_t i = 0; i < memlayout->count; i++) {
    // BIOS保留的内存
    if (!E820_ADDR_AVAILABLE(memlayout->ard[i].type)) {
      continue;
    }
    //高于4G
    if (memlayout->ard[i].addr >= 0xffffffff) {
      continue;
    }
    if (memlayout->ard[i].addr + memlayout->ard[i].size > 0xffffffff) {
      //如果尾端超出4G，那么截去超出部分
      total_memory += (uint32_t)((uint64_t)0xffffffff - memlayout->ard[i].addr);
    } else {
      total_memory += memlayout->ard[i].size;
    }
  }
  //现在计算normal_region应该有多大
  normal_region_size = total_memory / 4;
  // 1G-4M(boot stack用的)-normal_region=还剩多少虚拟地址空间
  uint32_t vm_left = 0x40000000 - 0x400000 - normal_region;
  if (normal_region_size > vm_left) {
    normal_region_size = vm_left;
  }
  normal_region_size = ROUNDDOWN(normal_region_size, _4M);
  assert(normal_region_size > 0);
  printf("kmem_init: total_memory=%d\n", total_memory);
  printf("kmem_init: normal_region=%d\n", normal_region);
  printf("kmem_init: normal_region_size=%d\n", normal_region_size);

  lcr3(V2P((uintptr_t)kernel_pd));
}
