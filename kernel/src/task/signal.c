#include "../../include/signal.h"

bool signal_fire(pid_t pid, bool group, int sig) { return false; }

bool signal_wait(pid_t pid, const sigset_t *set, int *sig) { return false; }

bool signal_set_handler(pid_t pid, int sig, void (*handler)(int)) {
  // 目前我们限制用户态进程只能设置自己的信号处理器
  // 只有内核态进程可以设置别的程序
  if (!task_current()->group->is_kernel) {
    if (pid != task_current()->id) {
      task_terminate(TASK_TERMINATE_INVALID_ARGUMENT);
    }
  }

  if (sig > SIGMAX || sig < SIGMIN) {
    task_terminate(TASK_TERMINATE_INVALID_ARGUMENT);
  }

  ktask_t *target = task_find(pid);
  if (!target) {
    return false;
  }
  target->signal_callback[sig] = (uintptr_t)handler;

  return true;
}
