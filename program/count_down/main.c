#include <compiler_helper.h>
#include <stdio.h>
#include <task.h>
#include <unistd.h>

#define SECONDS 5

int main(int argc, char **argv) {
  printf("parameter from kernel:\n");
  printf("argc: %d  argv: 0x%08llx\n", argc, (int64_t)(uintptr_t)argv);
  for (uint32_t i = 0; i < (uint32_t)argc; i++) {
    printf("argv[%d]: 0x%08llx %s\n", i, (int64_t)(uintptr_t)argv[i], argv[i]);
  }
  for (int i = 0; i < SECONDS; i++) {
    printf("pid %d: count down %d\n", get_pid(), SECONDS - i);
    sleep(1000);
  }
  printf("pid %d: count down 0\n", get_pid());
  return SECONDS;
}