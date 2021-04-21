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

void kentry(void) {
  /*
  实际上在编译器与loader之间有一个约定，就是loader要负责把bss段全部清0
  之前没有把bss清0，所以程序里的static变量没有被初始化，于是就产生了逻辑错误
  https://en.wikipedia.org/wiki/.bss#BSS_in_C

  另外，这里extern char[]也有讲究，不能写成extern char*
  如果写成了extern char*，那这些值会变成0xffff这样的东西，
  然后接下来memset往0xffff地址写的时候就会挂掉。
  这个我也想不通是怎么回事，因为我们都知道char[]和char*实际上是一个东西（虽然在语言层面不是）
  */
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
  //TODO 考虑一下嵌套中断
  sti();

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

  // printf("nonoOS:$ ");

  //

  task_schd();

  while (1) {
    hlt();
  }
}
