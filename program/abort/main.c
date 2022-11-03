#include <compiler_helper.h>
#include <stdio.h>
#include <stdlib.h>

#define SECONDS 5

int main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);
  printf("im going to abort!\n");
  abort();
  __unreachable;
}
