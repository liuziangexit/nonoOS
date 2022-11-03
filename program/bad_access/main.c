#include <compiler_helper.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>
#include <unistd.h>

#define SECONDS 5

void child() {
  printf("bad_access_child sleep\n");
  sleep(10000);
  printf("bad_access_child something goes wrong\n");
  abort();
  __unreachable;
}

int main(int argc, char **argv) {
  UNUSED(argc);
  UNUSED(argv);

  printf("(%d) creating bad_access_child\n", get_pid());
  if (0 ==
      create_task(0, 0, "bad_access_child", false, (uintptr_t)child, 0, 0)) {
    printf("unable to create thread\n");
  }
  if (0 ==
      create_task(0, 0, "bad_access_child", false, (uintptr_t)child, 0, 0)) {
    printf("unable to create thread\n");
  }
  if (0 ==
      create_task(0, 0, "bad_access_child", false, (uintptr_t)child, 0, 0)) {
    printf("unable to create thread\n");
  }
  if (0 ==
      create_task(0, 0, "bad_access_child", false, (uintptr_t)child, 0, 0)) {
    printf("unable to create thread\n");
  }
  if (0 ==
      create_task(0, 0, "bad_access_child", false, (uintptr_t)child, 0, 0)) {
    printf("unable to create thread\n");
  }

  sleep(1000);
  printf("bad_access_parent is doing the bad thing!\n");
  *(volatile int *)0;
  __unreachable;
}
