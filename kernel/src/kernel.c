#include "../test/bare_hashmap.h"
#include "../test/kmem_cache.h"
#include "../test/kmem_page.h"
#include "../test/ring_buffer.h"
#include "../test/task.h"
#include <cga.h>
#include <debug.h>
#include <defs.h>
#include <gdt.h>
#include <interrupt.h>
#include <kbd.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <picirq.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <task.h>
#include <tty.h>
#include <x86.h>

void kmain();

void kentry() {
  {
    extern uint32_t kernel_pd[];
    extern uint32_t program_end[];
    extern char bss_begin[], bss_end[];
    _Alignas(_4K) uint32_t temp_pd[1024];

    //确定kernel_pd是在bss外面的
    assert(V2P(kernel_pd) >= bss_end || V2P(kernel_pd) < bss_begin);

    //现在用的还是bootloader栈，要换成boot栈，它是内核映像结束之后第一个物理4M页
    //把它map到虚拟内存最后一个4M页上
    memcpy(temp_pd, kernel_pd, 4096);
    //加载临时页表因为temp_pd是在bootloader栈上，这部分是V=P，所以不需要V2P
    lcr3((uintptr_t)temp_pd);
    //现在增加一个boot stack的map
    uint32_t physical_stack =
        V2P(ROUNDUP((uint32_t)program_end, 4 * 1024 * 1024));
    //把这个物理大页map到虚拟地址最后一个大页的位置，也就是KERNEL_BOOT_STACK
    kernel_pd[1023] = (physical_stack & ~0x3FFFFF) | PTE_P | PTE_PS | PTE_W;
    lcr3(V2P((uintptr_t)kernel_pd));

    //在新栈上调kmain
    asm volatile("movl 0, %%ebp;movl $0xFFFFFFFF, %%esp;call kmain;" ::
                     : "memory");
    __builtin_unreachable();
  }
}

void kmain() {
  {
    uint32_t ebp;
    rebp(&ebp);
    *(uint32_t *)ebp = 0;
    *(uint32_t *)(ebp + 4) = 0;
  }
  //清bss https://en.wikipedia.org/wiki/.bss#BSS_in_C
  extern char bss_begin[], bss_end[];
  memset(bss_begin, 0, bss_end - bss_begin);

  gdt_init();
  pic_init();
  idt_init();
  kbd_init();
  terminal_init();
  print_e820();
  printf("\n");
  kmem_init(e820map);
  kmem_page_init();
  kmem_alloc_init();
  kmem_cache_init();
  printf("\n");
  task_init();
  task_test();

  // https://en.wikipedia.org/wiki/Code_page_437
  putchar(1);
  putchar(1);
  putchar(1);
  printf("Welcome...\n");
  printf("\n\n");

  //跑测试
  ring_buffer_test();
  kmem_page_test();
  bare_hashmap_test();
  kmem_cache_test();

  printf("\n\n");
  print_kernel_size();
  printf("\n");
  printf("\n\n");
  print_cur_status();
  printf("\n\n");

  // extern char _binary____program_hello_world_hello_exe_start[],
  //     _binary____program_hello_world_hello_exe_size[];
  // printf("program_hello_start: 0x%08x, size: %d\n\n",
  //        (uintptr_t)_binary____program_hello_world_hello_exe_start,
  //        (uint32_t)_binary____program_hello_world_hello_exe_size);

  printf("nonoOS:$ ");

  // TODO 考虑一下嵌套中断
  sti();

  task_schd();

  while (1) {
    hlt();
  }
  __builtin_unreachable();
}
