#include <compiler_helper.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>

int main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);
  printf("schd test %lld online!\n", (int64_t)get_pid());
  int32_t cnt = 9999;

  while (--cnt) {
    printf("schd_test pid %lld: %d\n", (int64_t)get_pid(), cnt);
  }
  return 555;
}