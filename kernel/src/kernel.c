#include "../test/ring_buffer.h"
#include <cga.h>
#include <defs.h>
#include <kbd.h>
#include <memlayout.h>
#include <mmu.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>

void kentry(void) {
  terminal_init();
  kbd_init();
  printf("Welcome...\n");
  printf("\n\n");
  ring_buffer_test();
  printf("\n\n");
  printf("nonoOS:$ ");
  while (1) {
    kbd_isr();
  }
  //
  while (1)
    ;
}
