#include "../../include/signal.h"
#include <compiler_helper.h>
#include <string.h>
#include <sync.h>

bool signal_fire(pid_t pid, bool group, int sig) {
  ktask_t *target = task_find(pid);
  if (!target) {
    return false;
  }

  {
    SMART_LOCK(l, target->signal_seq_mut);
    target->signal_fire_seq[sig - 1]++;
    condition_variable_notify_all(target->signal_seq_fire_cv,
                                  target->signal_seq_mut);
  }

  if (group) {
    for (list_entry_t *p = list_next(&target->group->tasks);
         p != &target->group->tasks; p = list_next(p)) {
      ktask_t *t = task_group_head_retrieve(p);
      if (t->id != target->id) {
        SMART_LOCK(l, t->signal_seq_mut);
        t->signal_fire_seq[sig - 1]++;
        condition_variable_notify_all(t->signal_seq_fire_cv, t->signal_seq_mut);
      }
    }
  }

  if (pid == task_current()->id) {
    signal_handle_on_task_schd_in();
  }

  return true;
}

bool signal_wait(pid_t pid, const sigset_t *set, int *sig) {
  ktask_t *target = task_find(pid);
  if (!target) {
    return false;
  }

  // 拷贝一个fire_seq放在这里
  // 然后等待fire_cv
  // 每次fire_cv发生时，检查是否有我们关注的信号被发出了
  // 也就是检查fire_seq里是否有某个我们关注的项已经比fire_seq_copy更大了

  SMART_LOCK(l, target->signal_seq_mut);

  uint32_t signal_fire_seq_copy[SIGMAX];
  memcpy(signal_fire_seq_copy, target->signal_fire_seq,
         sizeof(target->signal_fire_seq));

  while (true) {
    condition_variable_wait(target->signal_seq_fire_cv, target->signal_seq_mut,
                            true);
    for (int psig = SIGMIN; psig <= SIGMAX; psig++) {
      if (sigismember(set, psig) == 1) {
        if (target->signal_fire_seq[psig - 1] !=
            signal_fire_seq_copy[psig - 1]) {
          // 这个sig参数是我们返回发生了的信号的编号的方式
          *sig = psig;
          return true;
        }
      }
    }
  }
  __unreachable
}

bool signal_set_handler(pid_t pid, int sig, void (*handler)(int)) {
  // 不能捕获的信号
  if (sig == SIGKILL) {
    return false;
  }

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
  target->signal_callback[sig - 1] = (uintptr_t)handler;

  return true;
}

// FIXME 实际上用户程序的信号处理器在这里是以特权级别运行的，不可以这样
void signal_handle_on_task_schd_in() {
  ktask_t *current = task_current();
  SMART_LOCK(l, current->signal_seq_mut);
  for (int sig = SIGMIN; sig <= SIGMAX; sig++) {
    for (; current->signal_fire_seq[sig - 1] > current->signal_fin_seq[sig - 1];
         current->signal_fin_seq[sig - 1]++) {
      void (*handler)(int) = (void (*)(int))current->signal_callback[sig - 1];
      if (handler)
        handler(sig);
    }
  }
}

void default_signal_handler(int sig) {
  switch (sig) {
  case SIGINT: {
    printf("signal SIGINT(2) received, terminate program\n", sig);
    task_terminate(TASK_TERMINATE_QUIT_ABNORMALLY);
  } break;
  case SIGQUIT: {
    printf("signal SIGQUIT(3) received, terminate program\n", sig);
    task_terminate(TASK_TERMINATE_QUIT_ABNORMALLY);
  } break;
  case SIGABRT: {
    printf("Aborted\n");
    task_terminate(TASK_TERMINATE_ABORT);
  } break;
  case SIGKILL: {
    printf("signal SIGKILL(9) received, terminate program\n", sig);
    task_terminate(TASK_TERMINATE_QUIT_ABNORMALLY);
  } break;
  case SIGSEGV: {
    printf("Program received signal SIGSEGV, Segmentation fault.\n");
    task_terminate(TASK_TERMINATE_QUIT_ABNORMALLY);
  } break;
  case SIGTERM: {
    printf("signal SIGTERM(15) received, terminate program\n", sig);
    task_terminate(TASK_TERMINATE_QUIT_ABNORMALLY);
  } break;
  default: {
    printf("unknown signal %d, ignored by default_signal_handler\n", sig);
  } break;
  }
}
