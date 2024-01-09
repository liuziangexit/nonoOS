#include <compiler_helper.h>
#include <stdlib.h>

#ifndef LIBNO_USER

#include <panic.h>

void abort(void) { panic("kernel abort has been called"); }

#else

#include <signal.h>
#include <stdio.h>
#include <syscall.h>
#include <task.h>

void abort() {
  // syscall(SYSCALL_TASK, 1, USER_TASK_ACTION_ABORT);
  kill(get_pid(), SIGABRT);
}

void exit(int ret) { syscall(SYSCALL_TASK, 2, USER_TASK_ACTION_EXIT, ret); }

#endif
