//#include <stdio.h>
//#include <task.h>
#include <compiler_helper.h>
#include <stdint.h>
#include <syscall.h>

int main(int argc, char **argv) {
  uint32_t look = argc;
  look = argv;

  // UNUSED(argc);
  // UNUSED(argv);
  // int ret = syscall(99, 1, 2, 3, 4, 5);
  // printf("hello world from user process %d\n", task_current());
  return 0;
}