#include "../include/task.h"
#include <stdarg.h>
#include <syscall.h>

pid_t get_pid() { return syscall(SYSCALL_TASK, 1, USER_TASK_ACTION_GET_PID); }

pid_t create_task(void *program, uint32_t program_size, const char *name,
                  bool new_group, uintptr_t entry, ...) {
  struct create_task_syscall_args arg_pack;
  arg_pack.program = program;
  arg_pack.program_size = program_size;
  arg_pack.name = name;
  arg_pack.new_group = new_group;
  arg_pack.entry = entry;
  va_list parameters;
  va_start(parameters, entry);
  uint32_t ret =
      syscall(SYSCALL_TASK, 3, USER_TASK_ACTION_CREATE, &arg_pack, &parameters);
  va_end(parameters);
  return ret;
}
