#include <compiler_helper.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>

int main(int argc, char **argv) {
  printf("****hello world from user process %lld****\n", (int64_t)get_pid());
  printf("argc: %d\n", argc);
  for (int i = 0; i < argc; i++) {
    printf("%s ", argv[i]);
  }
  printf("\n");
  volatile uint32_t *mem = malloc(4);
  *mem = 999;
  volatile uint32_t *mem2 = malloc(4);
  *mem2 = 888;
  volatile uint32_t *mem3 = malloc(4);
  *mem3 = 777;
  volatile uint32_t *mem4 = malloc(4095);
  // FIXME 下面这一行内存访问将引发bug
  //*mem4 = 777;
  free((void *)mem);
  free((void *)mem2);
  free((void *)mem3);
  free((void *)mem4);
  return 888;
}