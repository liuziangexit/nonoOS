#include <cga.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>

void init_terminal() { terminal_init(CGA_COLOR_BLACK, CGA_COLOR_LIGHT_GREY); }

void init_paging() {}

void kentry(void) {
  init_terminal();
  printf("Welcome...\n");
  printf("Loading nonoOS...\n");
  printf("Initializing Paging...\n");
  const char *mes1 =
      "1suuuuuuppppppppperrrrrrrrrrlllllloooooonnnnnggggggggggg11"
      "111111111111111111111111111111111111111110\n";
  const char *mes2 =
      "2suuuuuuppppppppperrrrrrrrrrlllllloooooonnnnngggggggggggww"
      "wwwwewewewewewewe1111111111111111111119999";
  terminal_write_string(mes1);
  terminal_write_string(mes2);
  terminal_write_string("\naaa\n");
  printf("\naaa\n");
  init_paging();

  //
  while (1)
    ;
}
