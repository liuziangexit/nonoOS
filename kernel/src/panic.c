#include <panic.h>
#include <task.h>
#include <x86.h>

__attribute__((__noreturn__)) void panic(const char *message) {
  task_preemptive_set(false);
  printf_color(CGA_COLOR_RED, "kernel panic: %s\n", message);
  while (1) {
    hlt();
  }
  __unreachable;
}