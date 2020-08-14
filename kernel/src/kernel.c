#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>
#include <cga.h>

void init_terminal() {
  terminal_initialize(CGA_COLOR_BLACK, CGA_COLOR_LIGHT_GREY);
}

void init_paging() {}

void kentry(void) {
  init_terminal();
  printf("Welcome...\n");
  printf("Loading nonoOS...\n");
  printf("Initializing Paging...\n");
  init_paging();

  //
  while (1)
    ;
}
