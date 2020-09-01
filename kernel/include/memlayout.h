#ifndef __KERNEL_MEMLAYOUT_H__
#define __KERNEL_MEMLAYOUT_H__

/*
注意，这里的物理地址就是线性地址，因为我们有平坦的segementation
Physical Address                         |        Virtual Address        |
[0x0, 0x7C00)                            |    [0xC0000000, 0xC0007C00)   | bootloader栈
[0x7C00, 0x7E00)                         |    [0xC0007C00, 0xC0007E00)   | bootloader代码，512字节
[0x0, 0x100000)                          |    [0xC0000000, 0xC0100000)   | 保留区域，1MB
[0x100000, 0x800000)                     |    [0xC0100000, 0xC0800000)   | 内核代码区域，8MB
[0x800000, 0xC00000)                     |    [0xC0800000, 0xC0C00000)   | 内核栈，4M
[0xC00000, 内存上限)                      |                               |  可用区域
*/

//低1MB是保留区域
#define PRESERVED 0x100000
//内核虚拟基地址
#define KERNEL_VIRTUAL_BASE 0xC0000000
//内核被link到的虚拟地址
#define KERNEL_LINKED (KERNEL_VIRTUAL_BASE + PRESERVED)
//虚拟地址转物理地址
#define V2P(n) (n - KERNEL_VIRTUAL_BASE)
//物理地址转虚拟地址
#define P2V(n) (n + KERNEL_VIRTUAL_BASE)

//内核栈位置
#define KERNEL_STACK 0x800000
//内核栈大小
#define KERNEL_STACK_SIZE 0x400000
//可用区域
#define KERNEL_FREESPACE (KERNEL_STACK + KERNEL_STACK_SIZE)

#ifndef __ASSEMBLER__
#include <stdint.h>

// some constants for bios interrupt 15h AX = 0xE820
#define E820MAX 20 // number of entries in E820MAP
#define E820_ADDR_AVAILABLE(type) ((type == 1) ? 1 : 0)

struct e820map_t {
  int count;

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
