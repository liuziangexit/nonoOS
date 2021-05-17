//#include <stdio.h>
//#include <task.h>
#include <compiler_helper.h>
#include <stdint.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  volatile uint32_t *mem = malloc(4);
  *mem = 888;
  volatile uint32_t *mem2 = malloc(4);
  *mem2 = 999;
  free((void *)mem);
  // free(mem);
  free((void *)mem2);

  UNUSED(argc);
  UNUSED(argv);
  // printf("hello world from user process %d\n", task_current());
  return *mem + *mem2;
}