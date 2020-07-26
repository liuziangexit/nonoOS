#include <stdio.h>

#include <tty.h>
#include <vga_color.h>

void kentry(void) {
  terminal_initialize(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
  printf("nonoOS 5\n");

  //
  while (1)
    ;
}
