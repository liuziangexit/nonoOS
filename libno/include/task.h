#ifndef __LIBNO_TASK_H__
#define __LIBNO_TASK_H__

#define USER_TASK_ACTION_GET_PID 1
#define USER_TASK_ACTION_CREATE 2
#define USER_TASK_ACTION_YIELD 3
#define USER_TASK_ACTION_SLEEP 4
#define USER_TASK_ACTION_JOIN 5
#define USER_TASK_ACTION_EXIT 6

#ifdef LIBNO_USER
#include <stdbool.h>
#include <stdint.h>

typedef uint32_t pid_t;
pid_t get_pid();
#define DEFAULT_ENTRY 0
pid_t create_task(void *program, uint32_t program_size, const char *name,
                  bool new_group, uintptr_t entry, ...);
void yield();
void sleep();
void join();

struct create_task_syscall_args {
  void *program;
  uint32_t program_size;
  const char *name;
  bool new_group;
  uintptr_t entry;
};

#endif
#endif
