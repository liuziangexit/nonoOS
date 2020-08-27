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
