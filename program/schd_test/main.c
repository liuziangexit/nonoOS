#include <compiler_helper.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>

int main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);
  printf("schd test %lld online!\n", (int64_t)get_pid());
  while (1) {
    // printf("schd_test\n");
  }
}