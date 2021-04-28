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
  normal_region_vaddr = KERNEL_VIRTUAL_BASE + boot_stack_paddr + _4M;
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
  uint32_t vm_left = 0xffffffff - 0x400000 - normal_region_vaddr;
  if (normal_region_size > vm_left) {
    normal_region_size = vm_left;
  }
  normal_region_size = ROUNDDOWN(normal_region_size, _4M);
  assert(normal_region_size >= _4M);
  printf("kmem_init: total_memory       =0x%09llx\n", (int64_t)total_memory);
  printf("kmem_init: normal_region_vaddr=0x%09llx\n",
         (int64_t)normal_region_vaddr);
  printf("kmem_init: normal_region_size =0x%09llx\n",
         (int64_t)normal_region_size);

  //开始寻找合适的物理内存并map到这个normal_region
  normal_region_paddr = 0;
  for (uint32_t i = 0; i < memlayout->count; i++) {
    // BIOS保留的内存
    if (!E820_ADDR_AVAILABLE(memlayout->ard[i].type)) {
      continue;
    }
    //小于normal_region_size的内存
    if (memlayout->ard[i].size < normal_region_size) {
      continue;
    }
    //高于4G的内存
    if (memlayout->ard[i].addr >= 0xffffffff) {
      continue;
    }

    // 4M对齐
    char *addr = (char *)ROUNDUP((uintptr_t)memlayout->ard[i].addr, _4M);
    uint32_t round_diff = (uintptr_t)addr - (uintptr_t)memlayout->ard[i].addr;
    int32_t page_count = (int32_t)((memlayout->ard[i].size - round_diff) / _4M);
    //如果对齐之后发现凑不到要求的页数了，那这块内存就没用了
    if (page_count < normal_region_size / _4M) {
      continue;
    }
    //如果addr小于FREESPACE（其实就是物理地址12MB起的部分），就让他等于12MB
    if ((uintptr_t)addr < boot_stack_paddr + _4M) {
      if ((uint32_t)addr + (uint32_t)(page_count * _4M) >
          (uint32_t)boot_stack_paddr + _4M) {
        page_count -= ((boot_stack_paddr + _4M - (uintptr_t)addr) / _4M);
        addr = (char *)boot_stack_paddr + _4M;
      } else {
        continue;
      }
    }
    //如果addr大于等于4G，就忽略
    if ((uintptr_t)addr >= 0xFFFFFFFF) {
      continue;
    }
    //对于addr+size越过4G界的处理
    if ((uint32_t)addr + (uint32_t)(page_count * _4M) >= 0xFFFFFFFF) {
      assert((0xFFFFFFFF - (uint32_t)addr) % _4M == 0);
      page_count = (0xFFFFFFFF - (int32_t)addr) / _4M;
    }
    if (page_count < normal_region_size / _4M) {
      continue;
    }
    normal_region_paddr = addr;
    //好了，现在准备妥当了，开始做map
    pd_map(kernel_pd, normal_region_vaddr, normal_region_paddr, page_count,
           PTE_P | PTE_W | PTE_PS);
    break;
  }

  if (!normal_region_paddr) {
    panic("normal_region_paddr == 0");
  }
  printf("kmem_init: normal_region_paddr=0x%09llx\n",
         (int64_t)normal_region_paddr);

  lcr3(V2P((uintptr_t)kernel_pd));

  //验证是不是都可以访问
  for (uintptr_t p = normal_region_vaddr;
       p < normal_region_paddr + normal_region_size; p += 4) {
    *(volatile uint32_t *)p = 0xFFFFFFFF;
  }
  for (uintptr_t p = normal_region_vaddr;
       p < normal_region_paddr + normal_region_size; p += 4) {
    if (*(volatile uint32_t *)p != 0xFFFFFFFF) {
      panic("test normal_region failed");
    }
  }
}
