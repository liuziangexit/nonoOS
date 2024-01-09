#include <panic.h>
#include <sync.h>
#include <task.h>
#include <x86.h>

__attribute__((__noreturn__)) void panic(const char *message) {
  task_preemptive_set(false);
  printf_color(CGA_COLOR_RED, "kernel panic: %s\n", message);
  // 打开中断使得我们可以通过键盘操作
  enable_interrupt();
  while (1) {
    hlt();
  }
  __unreachable;
}