#include <panic.h>

__attribute__((__noreturn__)) void panic(const char *message) {
  terminal_fgcolor(CGA_COLOR_RED);
  printf("kernel panic: %s\n", message);
  terminal_default_color();
  while (1) {
  }
  __builtin_unreachable();
}