#ifndef __KERNEL_TASK_SIGNAL_H__
#define __KERNEL_TASK_SIGNAL_H__
#include "../../libno/include/signal.h"
#include "task.h"
#include <stdbool.h>

// group表示是否对pid所属组中每个task都发送
bool signal_fire(pid_t pid, bool group, int sig);

/*
       The sigwait() function suspends execution of the calling thread
       until one of the signals specified in the signal set set becomes
       pending.  The function returns the signal number in sig.

       Return value indicates whether the call is successful.
*/
bool signal_wait(pid_t pid, const sigset_t *set, int *sig);

void signal_set_handler(pid_t pid, int sig, void (*handler)(int));

#endif
