#include <compiler_helper.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <stdint.h>
#include <string.h>
#include <x86.h>

void kmem_init() {
  extern uint32_t kernel_page_directory[];
  //如果直接修改正在使用的页目录，会翻车
  //所以lcr3一个一模一样的页目录副本，这样我们才能开始修改真正的页目录
  _Alignas(4096) uint32_t temp_pd[1024];
  memcpy(temp_pd, kernel_page_directory, 4096);
  lcr3(V2P((uintptr_t)temp_pd));
  //好吧，现在开始修改真的页目录(kernel_page_directory)
  //虚拟地址0到4M -> 物理地址0到4M（为了CGA这样的外设地址）
  pd_map_ps(kernel_page_directory, 0, 0, 1, PTE_P | PTE_W | PTE_PS | PTE_U);
  //虚拟地址4M到3G -> 物理地址1G+4M到3G
  pd_map_ps(kernel_page_directory, _4M, 1024 * 1024 * 1024 + _4M, 768 - 1,
            PTE_P | PTE_W | PTE_PS | PTE_U);
  //虚拟地址3G到4G -> 物理地址0到1G
  pd_map_ps(kernel_page_directory, KERNEL_VIRTUAL_BASE, 0, 256,
            PTE_P | PTE_W | PTE_PS | PTE_U);
  //重新加载真的页目录(kernel_page_directory)
  lcr3(V2P((uintptr_t)kernel_page_directory));
}

void kmem_page_init(struct e820map_t *memlayout) { UNUSED(memlayout); }
void *kmem_page_alloc(size_t cnt);
void kmem_page_free(void *);
void kmem_page_dump();
