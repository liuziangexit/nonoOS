#include "../../kernel/include/signal.h"
#include <signal.h>
#include <syscall.h>
#include <task.h>

#ifndef LIBNO_USER

// 注册信号处理回调
void signal(int sig, void (*handler)(int)) {
  ktask_t *t = task_current();

  bool ok = signal_set_handler(t->id, sig, handler);
  if (!ok) {
    abort();
  }
}

// 向某个线程发送信号
int kill(pid_t pid, int sig) { return signal_fire(pid, false, sig) ? 0 : -1; }

// 向某个进程(task_group中的所有task)发送信号
int killpg(pid_t pid, int sig) { return signal_fire(pid, true, sig) ? 0 : -1; }

#else

// 注册信号处理回调
void signal(int sig, void (*handler)(int)) {
  syscall(SYSCALL_SIGNAL_SET_HANDLER, 3, get_pid(), sig, handler);
}

// 向某个线程发送信号
int kill(pid_t pid, int sig) {
  return (bool)syscall(SYSCALL_SIGNAL_FIRE, 3, pid, false, sig) ? 0 : -1;
}

// 向某个进程(task_group中的所有task)发送信号
int killpg(pid_t pid, int sig) {
  return (bool)syscall(SYSCALL_SIGNAL_FIRE, 3, pid, true, sig) ? 0 : -1;
}

#endif

// This function initializes the signal set set to exclude all of the defined
// signals. It always returns 0.
int sigemptyset(sigset_t *set) {
  UNUSED(set);
  return 0;
}

// This function initializes the signal set set to include all of the defined
// signals. Again, the return value is 0.
int sigfillset(sigset_t *set) {
  UNUSED(set);
  return 0;
}

// This function adds the signal signum to the signal set set.
// The return value is 0 on success and -1 on failure.
int sigaddset(sigset_t *set, int signum) {
  UNUSED(set);
  UNUSED(signum);
  return 0;
}

// This function removes the signal signum from the signal set set.
// The return value and error conditions are the same as for sigaddset.
int sigdelset(sigset_t *set, int signum) {
  UNUSED(set);
  UNUSED(signum);
  return 0;
}

// The sigismember function tests whether the signal signum is a member of the
// signal set set. It returns 1 if the signal is in the set, 0 if not, and -1 if
// there is an error.
int sigismember(const sigset_t *set, int signum) {
  UNUSED(set);
  UNUSED(signum);
  return 0;
}