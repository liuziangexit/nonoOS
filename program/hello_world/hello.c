#include <compiler_helper.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>

int main(int argc, char **argv) {
  printf("****hello world from user process %ll****\n", (int64_t)get_pid());
  volatile uint32_t *mem = malloc(4);
  *mem = argc;
  volatile uint32_t *mem2 = malloc(4);
  volatile uint32_t *mem3 = malloc(4);
  *mem2 = argv;
  free((void *)mem);
  free((void *)mem2);
  free((void *)mem3);

  UNUSED(argc);
  UNUSED(argv);
  // printf("hello world from user process %d\n", task_current());
  // return *mem + *mem2;
  return 888;
}