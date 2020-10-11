#include <stdint.h>
#include <stdio.h>
#include <task.h>

#ifdef NDEBUG
#define __NDEBUG_BEEN_FUCKED
#undef NDEBUG
#endif

void task2(void *arg) {
  printf("task2: i cant believe im alive! argument is %d\n", (intptr_t)arg);
  task_display();
  printf("task2: switching back...\n");
  task_switch((pid_t)arg);
  printf("task2: i cant believe im still alive!\n");
  task_display();
  printf("task2: exiting\n");
  task_exit();
}

void task_test() {
  printf("running task_test\n");

  printf("task1: creating task2\n");
  pid_t t2 = task_create(task2, (void *)task_current(), "test", true,
                         task_find(task_current())->group);
  assert(t2);
  pid_t t1 = task_current();
  assert(t1);
  task_display();
  printf("task1: switching to task2\n");
  task_switch(t2);
  printf("task1: switched back\n");
  task_display();
  printf("task1: switching to task2 second time\n");
  task_switch(t2);
  printf("task1: switched back second time, cool!\n");
  task_display();

  terminal_default_color();
  printf("tasktest passed!!!\n");
  terminal_default_color();

  destroy_exited();
}

#ifdef __NDEBUG_BEEN_FUCKED
#define NDEBUG
#undef __NDEBUG_BEEN_FUCKED
#endif
