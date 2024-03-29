#ifndef __LIBNO_TASK_H__
#define __LIBNO_TASK_H__

#define USER_TASK_ACTION_GET_PID 1
#define USER_TASK_ACTION_CREATE 2
#define USER_TASK_ACTION_YIELD 3
#define USER_TASK_ACTION_SLEEP 4
#define USER_TASK_ACTION_JOIN 5
#define USER_TASK_ACTION_EXIT 6
#define USER_TASK_ACTION_ABORT 7

#include <stdbool.h>

struct create_task_syscall_args {
  void *program;
  uint32_t program_size;
  const char *name;
  bool new_group;
  uintptr_t entry;
  bool ref; // 是否一开始就引用创建的线程
  uint32_t parameter_cnt;
};

#ifdef LIBNO_USER
#include <stdint.h>

typedef uint32_t pid_t;
pid_t get_pid();
#define DEFAULT_ENTRY 0
#define CREATE_TASK_REF 1
pid_t create_task(void *program, uint32_t program_size, const char *name,
                  bool new_group, uintptr_t entry, uint32_t flags,
                  uint32_t parameter_cnt, ...);
void yield();
void sleep();
// 返回INT_MIN说明失败
int32_t join(pid_t id);

#endif
#endif
