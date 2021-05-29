#include <stdarg.h>
#include <syscall.h>
#include <task.h>

pid_t get_pid() { return syscall(SYSCALL_TASK, 1, USER_TASK_ACTION_GET_PID); }

// pid_t create_task(void *program, uint32_t program_size, const char *name,
//                   bool new_group, uintptr_t entry, ...) {
//   va_list parameters;
//   va_start(parameters, args);
//   int ret;
//   va_end(parameters);
//   return ret;
// }
