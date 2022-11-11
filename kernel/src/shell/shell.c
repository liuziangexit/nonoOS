#include "shell.h"
#include "sync.h"
#include "task.h"
#include <atomic.h>
#include <compiler_helper.h>
#include <stdio.h>
#include <string.h>

uint32_t task_count();

static bool __sr = false;

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

  shell_set_fg(task_current()->id);
  __sr = true;

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

bool shell_ready() { return __sr; }

static pid_t fg;
void shell_set_fg(pid_t pid) { atomic_store(&fg, pid); }

pid_t shell_fg() { return atomic_load(&fg); }

int shell_execute_user(const char *name, int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);
  if (strcmp(name, "countdown")) {
    return 0;
  }
  return 0;
}
