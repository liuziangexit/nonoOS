#include <compiler_helper.h>
#include <stdlib.h>

#ifndef LIBNO_USER

#include <panic.h>

void abort(void) { panic("abort has been called"); }

void exit(int ret) {
  UNUSED(ret);
  panic("guaguaguagua");
}

#else

#include <stdio.h>
#include <syscall.h>
#include <task.h>

void abort(void) {
  printf("libno abort has been called");
  exit(-1);
}

void exit(int ret) { syscall(SYSCALL_TASK, 2, USER_TASK_ACTION_EXIT, ret); }

#endif
