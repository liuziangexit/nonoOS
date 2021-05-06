#include "../test/bare_hashmap.h"
#include "../test/kmem_cache.h"
#include "../test/kmem_page.h"
#include "../test/ring_buffer.h"
#include "../test/task.h"
#include <../test/atomic_test.h>
#include <cga.h>
#include <clock.h>
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

//用来测试程序映像很大的时候会怎么样
// static char fuck[1024 * 1024 * 32];

void kmain();
void ktask0();

//此函数用的是bootloader栈
void kentry() {
  extern uint32_t kernel_pd[];
  extern uint32_t program_begin[], program_end[];
  _Alignas(_4K) uint32_t temp_pd[1024];

  //切换到临时页表，因为temp_pd是在bootloader栈上，这部分是V=P，所以不需要V2P
  memcpy(temp_pd, kernel_pd, 4096);
  lcr3((uintptr_t)temp_pd);

  //现在只map了内核代码的前4M，我们需要map完整的内核映像
  for (uintptr_t p = ((uintptr_t)program_begin) + _4M;
       p < (uintptr_t)program_end; p += _4M) {
    kernel_pd[p >> PDXSHIFT] = V2P(p) | PTE_P | PTE_PS | PTE_W;
  }
  //虚拟地址0到16M -> 物理地址0到16M
  //虚拟地址3G到3G+16M -> 物理地址0到16M
  for (uintptr_t p = 0; p < _4M * 4; p += _4M) {
    kernel_pd[p >> PDXSHIFT] = p | PTE_P | PTE_PS | PTE_W;
    kernel_pd[P2V(p) >> PDXSHIFT] = p | PTE_P | PTE_PS | PTE_W;
  }
  //现在用的还是bootloader栈，要换成boot栈，它是内核映像结束之后第一个物理4M页
  //把它map到虚拟内存最后一个4M页上
  uintptr_t pstack = V2P(ROUNDUP((uint32_t)program_end, _4M));
  //把这个物理大页map到虚拟地址最后一个大页的位置，也就是KERNEL_BOOT_STACK
  kernel_pd[1023] = (pstack & ~0x3FFFFF) | PTE_P | PTE_PS | PTE_W;

  //切回kernel_pd
  lcr3(V2P((uintptr_t)kernel_pd));

  //确定kernel_pd是在bss外面的
  extern uint32_t kernel_pd[];
  extern char bss_begin[], bss_end[];
  assert((uintptr_t)V2P(kernel_pd) >= (uintptr_t)bss_end ||
         (uintptr_t)V2P(kernel_pd) < (uintptr_t)bss_begin);
  //清bss https://en.wikipedia.org/wiki/.bss#BSS_in_C
  extern char bss_begin[], bss_end[];
  memset(bss_begin, 0, bss_end - bss_begin);

  boot_stack_paddr = pstack;

  //在新栈上调kmain
  asm volatile("movl 0, %%ebp;movl $0xFFFFFFFF, %%esp;call kmain;" ::
                   : "memory");
  __builtin_unreachable();
}

//此函数用的是boot栈
void kmain() {
  gdt_init();
  idt_init();
  pic_init();
  kbd_init();
  terminal_init();
  print_e820();
  printf("\n");
  kmem_init(e820map);
  kmem_page_init();
  kmem_page_debug();
  kmem_alloc_init();
  kmem_cache_init();
  printf("\n");
  {
    extern uint32_t kernel_pd[];
    // page_directory_debug(kernel_pd);
  }

  task_init();

  uint32_t esp, ebp, new_esp, new_ebp;
  const uint32_t stack_top = KERNEL_VIRTUAL_BOOT_STACK + KERNEL_BOOT_STACK_SIZE;
  uint32_t used_stack, current_stack_frame_size;
  extern uint32_t kernel_pd[];
  _Alignas(4096) uint32_t temp_pd[1024];
  //将现在用的栈切换成这个内核任务的栈
  resp(&esp);
  rebp(&ebp);
  used_stack = stack_top - esp;
  current_stack_frame_size = ebp - esp;
  // FIXME find 0也能找到?
  struct ktask *init = task_find(1);
  new_esp = init->kstack + (TASK_STACK_SIZE * 4096 - used_stack);
  new_ebp = new_esp + current_stack_frame_size;

  memcpy((void *)new_esp, (void *)esp, used_stack);
  //然后unmap清空原来的内核栈，这样如果还有代码访问那个地方，就会被暴露出来
  memcpy(temp_pd, kernel_pd, _4K);
  lcr3(pd_lookup(kernel_pd, (uintptr_t)temp_pd));
  kernel_pd[1023] = 0;
  //在任务栈上调ktask0
  asm volatile("movl %0, %%ebp;movl %1, %%esp;call ktask0;"
               :
               : "r"(new_ebp), "r"(new_esp)
               : "memory");
  __builtin_unreachable();
}

//此函数用的是task1的任务栈
void ktask0() {
  {
    extern uint32_t kernel_pd[];
    lcr3(V2P((uintptr_t)kernel_pd));
    //将old ebp和ret address设为0
    uint32_t ebp;
    rebp(&ebp);
    *(uint32_t *)ebp = 0;
    *(uint32_t *)(ebp + 4) = 0;
  }
  // task_test();
  // // https://en.wikipedia.org/wiki/Code_page_437
  putchar(1);
  putchar(1);
  putchar(1);
  printf("Welcome...\n");
  printf("\n\n");

  {
    extern uint32_t kernel_pd[];
    // page_directory_debug(kernel_pd);
  }

  // //跑测试
  ring_buffer_test();
  kmem_page_test();
  bare_hashmap_test();
  kmem_cache_test();
  atomic_test();

  printf("\n\n");
  print_kernel_size();
  printf("\n");
  printf("\n\n");
  print_cur_status();
  printf("\n\n");

  // // extern char _binary____program_hello_world_hello_exe_start[],
  // //     _binary____program_hello_world_hello_exe_size[];
  // // printf("program_hello_start: 0x%08x, size: %d\n\n",
  // //        (uintptr_t)_binary____program_hello_world_hello_exe_start,
  // //        (uint32_t)_binary____program_hello_world_hello_exe_size);

  printf("nonoOS:$ ");

  clock_init();
  sti();

  // task_schd();

  while (1) {
    hlt();
  }
  __builtin_unreachable();
}
