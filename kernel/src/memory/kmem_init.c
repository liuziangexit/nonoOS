#include <assert.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <string.h>
#include <x86.h>

void kmem_init(struct e820map_t *memlayout) {
  extern uint32_t kernel_page_directory[];
  //如果直接修改正在使用的页目录，会翻车
  //所以lcr3一个一模一样的页目录副本，这样我们才能开始修改真正的页目录
  _Alignas(_4K) uint32_t temp_pd[1024];
  memcpy(temp_pd, kernel_page_directory, _4K);
  union {
    struct CR3 cr3;
    uintptr_t val;
  } cr3;
  cr3.val = 0;
  set_cr3(&(cr3.cr3), V2P((uintptr_t)temp_pd), false, false);
  lcr3(cr3.val);
  //好吧，现在开始修改真的页目录(kernel_page_directory)
  memset(kernel_page_directory, 0, _4K);
  //虚拟地址0到4M -> 物理地址0到4M（为了CGA这样的外设地址）
  pd_map_ps(kernel_page_directory, 0, 0, 1, PTE_P | PTE_W | PTE_PS);
  // 8M内核映像
  pd_map_ps(kernel_page_directory, KERNEL_VIRTUAL_BASE, 0, 2,
            PTE_P | PTE_W | PTE_PS);
  // 4M内核栈
  pd_map_ps(kernel_page_directory, KERNEL_VIRTUAL_BASE + KERNEL_STACK,
            KERNEL_STACK, 1, PTE_P | PTE_W | PTE_PS);
  /*
  处理 up to 2G 的 free space
  为什么不是3G呢？因为0-1MB那块硬件区域让这件事变得比较复杂，现在不想搞这种细节，以后再说

  这块区域，物理上是在kernel stack之后(12MB)，直到物理内存结束(最多2G+12MB)，
  我们把它map到虚存12MB的位置
  （为啥是12MB这个地方？因为这样的话就跟物理地址一一对应了，这样也是为了少处理一些细节
   */

  //下面这个for语句的框架是从kmem_page_alloc里面偷来的，如果要改这里，看看要不要把那边一块改了
  //这里跟那里的一个区别在于，这里处理的是大页，那里是小页，需要注意
  for (uint32_t i = 0; i < memlayout->count; i++) {
    // BIOS保留的内存
    if (!E820_ADDR_AVAILABLE(memlayout->ard[i].type)) {
      continue;
    }
    //小于1页的内存
    if (memlayout->ard[i].size < _4M) {
      continue;
    }
    //高于4G的内存，只可远观不可亵玩焉
    if (memlayout->ard[i].addr > 0xfffffffe) {
      continue;
    }

    // 4M对齐
    char *addr = ROUNDUP((char *)(uintptr_t)memlayout->ard[i].addr, _4M);
    uint32_t round_diff = (uintptr_t)addr - (uintptr_t)memlayout->ard[i].addr;
    int32_t page_count = (int32_t)((memlayout->ard[i].size - round_diff) / _4M);
    //如果对齐之后发现凑不到1页了，那这块内存就没用了
    if (page_count == 0) {
      continue;
    }
    //如果addr小于FREESPACE（其实就是物理地址12MB起的部分），就让他等于12MB
    if ((uintptr_t)addr < KERNEL_FREESPACE) {
      if ((uint32_t)addr + (uint32_t)(page_count * _4M) >
          (uint32_t)KERNEL_FREESPACE) {
        page_count -= ((KERNEL_FREESPACE - (uintptr_t)addr) / _4M);
        addr = (char *)KERNEL_FREESPACE;
        assert(page_count > 0);
      } else {
        continue;
      }
    }
    //对于addr+size越过2G+12M界的处理
    if ((uint32_t)addr + (uint32_t)(page_count * _4M) >= 0x80C00000) {
      assert((0x80C00000 - (uint32_t)addr) % _4M == 0);
      page_count = (0x80C00000 - (uint32_t)addr) / _4M;
      if (page_count == 0)
        continue;
    }
    //好了，现在准备妥当了，开始做map
    pd_map_ps(kernel_page_directory, (uintptr_t)addr, (uintptr_t)addr,
              page_count, PTE_P | PTE_W | PTE_PS);
  }

  //重新加载真的页目录(kernel_page_directory)
  cr3.val = 0;
  set_cr3(&(cr3.cr3), V2P((uintptr_t)kernel_page_directory), false, false);
  lcr3(cr3.val);
}
