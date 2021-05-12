#include <assert.h>
#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <sync.h>
#include <task.h>
#include <tty.h>

#ifdef NDEBUG
#define __NDEBUG_BEEN_FUCKED
#undef NDEBUG
#endif

void task2(int argc, char **argv) {
  printf("task2: i cant believe im alive!\n");
  printf("argc: %d\n", argc);
  for (int i = 0; i < argc; i++) {
    printf("%s ", argv[i]);
  }
  printf("\n");
  task_display();
  printf("task2: switching back...\n");
  disable_interrupt();
  task_switch(task_find(1));
  printf("task2: i cant believe im still alive!\n");
  task_display();
  printf("task2: exiting\n");
  disable_interrupt();
  task_exit();
}

void utask_test() {
  printf("\n\n******************\n");
  printf("user task test begin\n");
  printf("******************\n\n");

  extern char _binary____program_hello_world_hello_exe_start[],
      _binary____program_hello_world_hello_exe_size[];

  struct task_args *args = (struct task_args *)malloc(sizeof(struct task_args));
  task_args_init(args);
  task_args_add(args, "1");
  task_args_add(args, "2");
  task_args_add(args, "3");
  pid_t upid =
      task_create_user(_binary____program_hello_world_hello_exe_start,
                       (uintptr_t)_binary____program_hello_world_hello_exe_size,
                       "user_test_proc", 0, DETECT_ENTRY, args);
  printf("switch to user process...\n");
  disable_interrupt();
  task_switch(task_find(upid));
  printf("kernel task back!\n");
}

void task_test() {
  printf("running task_test\n");
  printf("task1: creating task2\n");
  struct task_args *args = (struct task_args *)malloc(sizeof(struct task_args));
  task_args_init(args);
  task_args_add(args, "11");
  task_args_add(args, "22");
  task_args_add(args, "33");
  pid_t t2 = task_create_kernel(task2, "kernel_test_proc", args);
  assert(t2);
  pid_t t1 = task_current();
  assert(t1);
  task_display();
  printf("task1: switching to task2\n");
  disable_interrupt();
  task_switch(task_find(t2));
  printf("task1: switched back\n");
  task_display();
  printf("task1: switching to task2 second time\n");
  disable_interrupt();
  task_switch(task_find(t2));
  printf("task1: switched back second time, cool!\n");
  task_display();

  utask_test();

  task_clean();

  terminal_default_color();
  printf("tasktest passed!!!\n");
  terminal_default_color();
}

#ifdef __NDEBUG_BEEN_FUCKED
#define NDEBUG
#undef __NDEBUG_BEEN_FUCKED
#endif
