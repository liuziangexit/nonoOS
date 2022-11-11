#include "shell.h"
#include "sync.h"
#include "task.h"
#include <compiler_helper.h>
#include <stdio.h>

uint32_t task_count();

int shell_main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);

  // waiting all other task to quit
  {
    disable_interrupt();
    while (task_count() != 2) {
      enable_interrupt();
      task_sleep(1050);
      disable_interrupt();
    }
    enable_interrupt();
  }

  task_display();

  // https://en.wikipedia.org/wiki/Code_page_437
  printf("System was compiled at " __DATE__ ", " __TIME__ ".\n");
  printf("nonoOS has been loaded. ");
  putchar(1);
  putchar(1);
  putchar(1);
  printf("\n\n");

  printf_color(CGA_COLOR_DARK_GREY, "nonoOS:$ ");

  while (true) {
    task_sleep(50);
  }
  __unreachable
}