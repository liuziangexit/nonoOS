#include <assert.h>
#include <compiler_helper.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>

int main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);
  printf("this is a user mode program!!!\n\n");
  printf("input something: ");
  char str[256];
  char *look = gets(str);
  assert(look == str);
  printf("\nyou have entered: %s\n\nnow, calling getchar()...\n", str);
  char c = getchar();
  printf("\n\nyou have pressed: %c\n\npress any key to exit...", c);
  getchar();
  return 0;
}