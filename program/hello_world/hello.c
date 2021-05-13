//#include <stdio.h>
//#include <task.h>
#include <compiler_helper.h>
#include <stdint.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  uint32_t *mem = malloc(4);
  // *mem = 9710;
  // free(mem);

  UNUSED(argc);
  UNUSED(argv);
  // printf("hello world from user process %d\n", task_current());
  return 0;
}