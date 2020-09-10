#include <memory_manager.h>
#include <mmu.h>
#include <string.h>
#include <x86.h>

void kmem_init() {
  extern uint32_t kernel_page_directory[];
  //如果直接修改正在使用的页目录，会翻车
  //所以lcr3一个一模一样的页目录副本，这样我们才能开始修改真正的页目录
  _Alignas(4096) uint32_t temp_pd[1024];
  memcpy(temp_pd, kernel_page_directory, 4096);
  union {
    struct CR3 cr3;
    uintptr_t val;
  } cr3;
  cr3.val = 0;
  set_cr3(&(cr3.cr3), V2P((uintptr_t)temp_pd), false, false);
  lcr3(cr3.val);
  //好吧，现在开始修改真的页目录(kernel_page_directory)
  memset(kernel_page_directory, 0, 4096);
  //虚拟地址0到4M -> 物理地址0到4M（为了CGA这样的外设地址）
  pd_map_ps(kernel_page_directory, 0, 0, 1, PTE_P | PTE_W | PTE_PS);
  // 8M内核映像
  pd_map_ps(kernel_page_directory, KERNEL_VIRTUAL_BASE, 0, 2,
            PTE_P | PTE_W | PTE_PS);
  // 4M内核栈
  pd_map_ps(kernel_page_directory, KERNEL_VIRTUAL_BASE + KERNEL_STACK,
            KERNEL_STACK, 1, PTE_P | PTE_W | PTE_PS);
  // 2G free space，这个物理上是在kernel image和kernel
  // stack之后，直到物理内存结束的这块区域，
  // 把它map到虚拟内存12MB的位置
  pd_map_ps(kernel_page_directory, KERNEL_FREESPACE, KERNEL_FREESPACE, 512,
            PTE_P | PTE_W | PTE_PS);

  //重新加载真的页目录(kernel_page_directory)
  cr3.val = 0;
  set_cr3(&(cr3.cr3), V2P((uintptr_t)kernel_page_directory), false, false);
  lcr3(cr3.val);
}
