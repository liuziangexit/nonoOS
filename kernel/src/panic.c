#include <panic.h>
#include <task.h>
#include <x86.h>

__attribute__((__noreturn__)) void panic(const char *message) {
  task_preemptive_set(false);
  terminal_fgcolor(CGA_COLOR_RED);
  printf("kernel panic: %s\n", message);
  terminal_default_color();
  while (1) {
    hlt();
  }
  __builtin_unreachable();
}