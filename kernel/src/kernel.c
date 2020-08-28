#include "../test/ring_buffer.h"
#include <cga.h>
#include <defs.h>
#include <gdt.h>
#include <interrupt.h>
#include <kbd.h>
#include <memlayout.h>
#include <mmu.h>
#include <picirq.h>
#include <stdio.h>
#include <string.h>
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
  sti();
  terminal_init();
  printf("Welcome...\n");
  printf("\n\n");
  ring_buffer_test();
  printf("\n\n");
  printf("nonoOS:$ ");
  //
  while (1)
    ;
}
