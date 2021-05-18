#ifndef __KERNEL_MEMLAYOUT_H__
#define __KERNEL_MEMLAYOUT_H__

#define KERNEL_VIRTUAL_BASE 0xC0000000
// DMA从0到16MB
#define DMA_REGION 0x0
#define DMA_REGION_SIZE 0x1000000
// 内核代码从16MB开始
#define KERNEL_IMAGE 0x1000000

// NORMAL_REGION是动态的
// 见后面的代码

// 内核MAP的地方从896MB开始
// 这个区域的主要作用是把FREE SPACE的某个或多个页map到这里，以便内核访问
// 这个只有加上了KERNEL_VIRTUAL_BASE才有意义
#define KERNEL_MAP_REGION 0x38000000
#define KERNEL_MAP_REGION_SIZE (0x40000000 - 0x38000000)

//虚拟地址转物理地址
#define V2P(n) (n - KERNEL_VIRTUAL_BASE)
//物理地址转虚拟地址
#define P2V(n) (n + KERNEL_VIRTUAL_BASE)

#ifndef __ASSEMBLER__
#include <stdbool.h>
#include <stdint.h>

/*
注意，这个“内核栈”是在task系统初始化之前用的，task系统初始化之后，
当前的控制流将被视为系统内的首个线程（下文称为init），此处内核栈的内容将被拷贝到
init的线程栈上，并且在那个线程栈上继续执行。
到那时，这个内核栈就没有用了
*/
#define KERNEL_BOOT_STACK_SIZE (4 * 1024 * 1024)
#define KERNEL_VIRTUAL_BOOT_STACK (1023 << 22)
extern uintptr_t boot_stack_paddr;
uintptr_t boot_stack_v2p(uintptr_t);
uintptr_t boot_stack_p2v(uintptr_t);

// NORMAL REGION
extern uintptr_t normal_region_vaddr;
extern uint32_t normal_region_size;
extern uintptr_t normal_region_paddr;

// some constants for bios interrupt 15h AX = 0xE820
#define E820MAX 20 // number of entries in E820MAP
#define E820_ADDR_AVAILABLE(type) ((type == 1) ? 1 : 0)

struct e820map_t {
  uint32_t count;

  // Address Range Descriptor
  struct {
    uint64_t addr;
    uint64_t size;
    uint32_t type;
  } __attribute__((packed)) ard[E820MAX];
};

extern struct e820map_t *e820map;

void print_e820();

#endif

#endif
