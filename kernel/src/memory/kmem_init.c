#include <assert.h>
#include <cga.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>
#include <x86.h>

//这里实现DMA_REGION和NORMAL_REGION到内核的映射
// DMA_REGION是boot stack之后的16MB内存
// NORMAL_REGION是DMA_REGION之后的剩余内存中的1/4
//例：总共有255MB内存，最初4MB是程序映像，第二个4MB是boot
// stack，接着是16MB的DMA_REGION，
//然后是(255-4-4-16)/4=57MB的NORMAL_REGION，剩下的区域是FREESACE，用于VMA
void kmem_init(struct e820map_t *memlayout) {
  extern uint32_t kernel_pd[];
  _Alignas(_4K) uint32_t temp_pd[1024];
  memcpy(temp_pd, kernel_pd, _4K);
  lcr3(boot_stack_v2p((uintptr_t)temp_pd));
  /*
  1)DMA_REGION
  2)程序映像的映射，从16MB到3G+16MB
  3)boot stack的映射
  */
  lcr3(V2P((uintptr_t)kernel_pd));
}
