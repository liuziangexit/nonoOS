#include <compiler_helper.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>

int main(int argc, char **argv) {
  UNUSED(argc);
  bool should_yield = false;
  if (argv[0][0] == '1')
    should_yield = true;
  printf("schd_test pid %lld: begin\n", (int64_t)get_pid());
  int32_t cnt = 1200;

  while (--cnt) {
    printf("schd_test pid %lld: %d\n", (int64_t)get_pid(), cnt);
    if (should_yield) {
      printf("schd_test pid %lld: yield\n", (int64_t)get_pid());
      yield();
    }
  }
  printf("schd_test pid %lld: exit\n", (int64_t)get_pid());
  return 555;
}