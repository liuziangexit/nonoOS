#include <compiler_helper.h>
#include <stdlib.h>

#ifndef LIBNO_USER

#include <panic.h>

void abort(void) { panic("kernel abort has been called"); }

#else

#include <stdio.h>
#include <syscall.h>
#include <task.h>

void abort() {
  printf("libno abort has been called");
  syscall(SYSCALL_TASK, 1, USER_TASK_ACTION_ABORT);
}

void exit(int ret) { syscall(SYSCALL_TASK, 2, USER_TASK_ACTION_EXIT, ret); }

#endif
