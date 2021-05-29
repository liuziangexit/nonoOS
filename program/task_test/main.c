#include <compiler_helper.h>
#include <shared_memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>

int main(int argc, char **argv) {
  printf("task_test: begin\n");
  union {
    uint32_t integer;
    unsigned char str[4];
  } punning;
  // 测一下内核传进来的共享内存
  memcpy(&punning.str, argv[0], 4);
  printf("task_test: shared memory id is %lld\n", (int64_t)punning.integer);
  void *vaddr = shared_memory_map(punning.integer, 0);
  printf("task_test: shared memory map to 0x%08llx\n",
         (int64_t)(uintptr_t)vaddr);
  printf("task_test: shared memory content: %s\n", vaddr);
  // shared_memory_unmap(vaddr);
  memcpy(&punning.str, argv[1], 4);
  const char *prog = (const char *)punning.integer;
  memcpy(&punning.str, argv[2], 4);
  const uint32_t prog_size = punning.integer;
  printf("program size: %lld\n", (int64_t)prog_size);
  pid_t pid =
      create_task(prog, prog_size, vaddr, true, DEFAULT_ENTRY,
                  "I AM task_test 1", "I AM task_test 2", "I AM task_test 3");
  printf("task_test: exit\n");
  return 0;
}