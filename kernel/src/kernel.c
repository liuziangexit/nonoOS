#include <cga.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>
#include "../test/ring_buffer.h"

void init_terminal() { terminal_init(CGA_COLOR_BLACK, CGA_COLOR_LIGHT_GREY); }

void init_paging() {}

void kentry(void) {
  init_terminal();
  printf("Welcome...\n");
  printf("Loading nonoOS...\n");
  printf("Initializing Paging...\n");
  init_paging();
  printf("\n\n");
  ring_buffer_test();

  //
  while (1)
    ;
}
