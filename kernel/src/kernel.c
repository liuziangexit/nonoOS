#include <stdio.h>
#include <memlayout.h>
#include <tty.h>
#include <vga_color.h>

void init_terminal() {
  terminal_initialize(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
}

void init_paging() {

}

void kentry(void) {
  init_terminal();
  printf("Welcome...\n");
  printf("Initializing nonoOS...\n");
  init_paging();
  printf("Paging enabled\n");
  

  //
  while (1)
    ;
}
