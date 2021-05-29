#include <compiler_helper.h>
#include <shared_memory.h>
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
  if (argc > 0) {
    // 测一下内核传进来的共享内存
    memcpy(&punning.str, argv[0], 4);
    printf("task_test: shared memory id is %lld\n", (int64_t)punning.integer);
    void *vaddr = shared_memory_map(punning.integer, 0);
    printf("task_test: shared memory map to 0x%08llx\n",
           (int64_t)(uintptr_t)vaddr);
    printf("task_test: shared memory content: %s\n", vaddr);
    // shared_memory_unmap(vaddr);
  }
  memcpy(&punning.str, argv[1], 4);
  const char *prog = punning.integer;
  memcpy(&punning.str, argv[2], 4);
  const uint32_t prog_size = punning.integer;
  printf("program size: %lld\n", (int64_t)prog_size);
  // struct task_args args;
  // task_args_init(&args);
  // task_args_add(&args, "I AM KERNEL!\n", 0, false);
  // task_create_user((void *)_binary____program_count_down_main_exe_start,
  //                  (uint32_t)_binary____program_count_down_main_exe_size,
  //                  "count_down", 0, DEFAULT_ENTRY, &args);
  // task_args_destroy(&args, true);
  printf("task_test: exit\n");
  return 0;
}