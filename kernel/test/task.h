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

void task2(void *arg) {
  printf("task2: i cant believe im alive! argument is %d\n", (intptr_t)arg);
  task_display();
  printf("task2: switching back...\n");
  disable_interrupt();
  task_switch((ktask_t *)arg);
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

  pid_t upid =
      task_create_user(_binary____program_hello_world_hello_exe_start,
                       (uintptr_t)_binary____program_hello_world_hello_exe_size,
                       "user", 0, DETECT_ENTRY, 0);
  printf("switch to user process...\n");
  disable_interrupt();
  task_switch(task_find(upid));
  printf("kernel task back!\n");
}

void task_test() {
  printf("running task_test\n");
  printf("task1: creating task2\n");
  pid_t t2 =
      task_create_kernel(task2, (void *)task_find(task_current()), "test");
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
