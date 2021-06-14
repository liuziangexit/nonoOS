#include "../../include/syscall.h"
#include "../../../libno/include/syscall.h"
#include "../../../libno/include/task.h"
#include "debug.h"
#include <condition_variable.h>
#include <mutex.h>
#include <panic.h>
#include <shared_memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <sync.h>
#include <task.h>
#include <tty.h>
#include <virtual_memory.h>
#include <x86.h>

//#define VERBOSE

int printf_impl(void *format, void *parameters);

void syscall_return(struct trapframe *tf, uint32_t ret) {
  tf->tf_gprs.reg_eax = ret;
}

void syscall_dispatch(struct trapframe *tf) {
  uint32_t arg[5] = {tf->tf_gprs.reg_edx, tf->tf_gprs.reg_ecx,
                     tf->tf_gprs.reg_ebx, tf->tf_gprs.reg_edi,
                     tf->tf_gprs.reg_esi};
  int no = tf->tf_gprs.reg_eax;
  ktask_t *const task = task_current();
  assert(task && !task->group->is_kernel);

  switch (no) {
  case SYSCALL_TASK: {
    uint32_t action = arg[0];
    switch (action) {
    case USER_TASK_ACTION_GET_PID: {
      syscall_return(tf, task->id);
    } break;
    case USER_TASK_ACTION_CREATE: {
      struct create_task_syscall_args *arg_pack =
          (struct create_task_syscall_args *)arg[1];
      va_list *parmaters = (va_list *)arg[2];
      task_group_t *group = 0;
      if (!arg_pack->new_group) {
        group = task->group;
      }
      struct task_args args;
      task_args_init(&args);
      for (uint32_t i = 0; i < arg_pack->parameter_cnt; i++) {
        task_args_add(&args, va_arg(*parmaters, const char *), 0, false);
      }
      pid_t id = task_create_user(
          arg_pack->program, arg_pack->program_size, arg_pack->name, group,
          arg_pack->entry, arg_pack->ref, arg_pack->parameter_cnt ? &args : 0);
      task_args_destroy(&args, true);
      syscall_return(tf, id);
    } break;
    case USER_TASK_ACTION_YIELD: {
      task_yield();
    } break;
    case USER_TASK_ACTION_SLEEP: {
      uint64_t ms = arg[1];
      ms <<= 32;
      ms |= arg[2];
      task_sleep(ms);
      syscall_return(tf, 0);
    } break;
    case USER_TASK_ACTION_JOIN: {
      int32_t ret;
      bool s = task_join(arg[1], &ret);
      if (s)
        syscall_return(tf, ret);
      else
        task_terminate(TASK_TERMINATE_JOIN_FAILED);
    } break;
    case USER_TASK_ACTION_EXIT: {
      task_exit(arg[1]);
    } break;
    case USER_TASK_ACTION_ABORT: {
      task_terminate(TASK_TERMINATE_ABORT);
    } break;
    default:
      panic("TODO USER ABORT!");
    }
  } break;
  case SYSCALL_ALLOC: {
#ifdef VERBOSE
    terminal_fgcolor(CGA_COLOR_LIGHT_YELLOW);
    printf("aligned_alloc() with args: %d, %d, %d, %d, %d\n", arg[0], arg[1],
           arg[2], arg[3], arg[4]);
    terminal_default_color();
#endif
    // arg[0]也就是alignment不需要用到，因为umalloc直接是平台最大对齐的
    uintptr_t vaddr = umalloc(task->group->vm, arg[1], true, 0, 0);
#ifdef VERBOSE
    terminal_fgcolor(CGA_COLOR_LIGHT_YELLOW);
    printf("aligned_alloc() returned: 0x%09llx\n", (int64_t)vaddr);
    terminal_default_color();
#endif
    syscall_return(tf, vaddr);
  } break;
  case SYSCALL_FREE: {
#ifdef VERBOSE
    terminal_fgcolor(CGA_COLOR_LIGHT_YELLOW);
    printf("free() with args: %d, %d, %d, %d, %d\n", arg[0], arg[1], arg[2],
           arg[3], arg[4]);
    terminal_default_color();
#endif
    ufree(task->group->vm, arg[0]);
    syscall_return(tf, 0);
  } break;
  case SYSCALL_PRINTF: {
    int ret = printf_impl((void *)arg[0], (void *)arg[1]);
    syscall_return(tf, ret);
  } break;
  case SYSCALL_SHM: {
    uint32_t action = arg[0];
    switch (action) {
    case USER_SHM_ACTION_CREATE: {
      uint32_t id = shared_memory_create((size_t)arg[1]);
      syscall_return(tf, id);
    } break;
    case USER_SHM_ACTION_MAP: {
      void *vaddr = shared_memory_map(arg[1], (void *)arg[2]);
      syscall_return(tf, (uint32_t)vaddr);
    } break;
    case USER_SHM_ACTION_UNMAP: {
      shared_memory_unmap((void *)arg[1]);
      syscall_return(tf, 0);
    } break;
    default:
      panic("TODO USER ABORT!");
    }
  } break;
  case SYSCALL_MTX: {
    uint32_t action = arg[0];
    if (action != USER_MTX_ACTION_CREATE && arg[1] == 0) {
      task_terminate(TASK_TERMINATE_INVALID_ARGUMENT);
    }
    switch (action) {
    case USER_MTX_ACTION_CREATE: {
      // printf("mutex create\n");
      int32_t obj = (int32_t)mutex_create();
      // printf("mutex create return %d\n", obj);
      syscall_return(tf, obj);
    } break;
    case USER_MTX_ACTION_LOCK: {
      // printf("mutex lock\n");
      mutex_lock(arg[1]);
      syscall_return(tf, 0);
    } break;
    case USER_MTX_ACTION_TIMEDLOCK: {
      // printf("mutex timedlock\n");
      uint64_t ms = arg[2];
      ms <<= 32;
      ms |= arg[3];
      // printf("timed lock: %lldms\n", (int64_t)ms);
      bool ret = mutex_timedlock(arg[1], ms);
      syscall_return(tf, (int32_t)ret);
    } break;
    case USER_MTX_ACTION_TRYLOCK: {
      // printf("mutex trylock\n");
      bool ret = mutex_trylock(arg[1]);
      syscall_return(tf, (int32_t)ret);
    } break;
    case USER_MTX_ACTION_UNLOCK: {
      // printf("mutex unlock\n");
      mutex_unlock(arg[1]);
      syscall_return(tf, 0);
    } break;
    default:
      panic("TODO USER ABORT!");
    }
  } break;
  case SYSCALL_CV: {
    uint32_t action = arg[0];
    switch (action) {
    case USER_CV_ACTION_CREATE: {
      printf("condition_variable_create\n");
      int32_t obj = (int32_t)condition_variable_create();
      syscall_return(tf, obj);
    } break;
    case USER_CV_ACTION_WAIT: {
      printf("condition_variable_wait\n");
      condition_variable_wait(arg[1], arg[2]);
      syscall_return(tf, 0);
    } break;
    case USER_CV_ACTION_TIMEDWAIT: {
      uint32_t cv = arg[1], mut = arg[2];
      uint64_t ms = arg[3];
      ms <<= 32;
      ms |= arg[4];
      printf("condition_variable_timedwait\n");
      // printf("cv timedwait: %lldms\n", (int64_t)ms);
      bool ret = condition_variable_timedwait(cv, mut, ms);
      syscall_return(tf, (int32_t)ret);
    } break;
    case USER_CV_ACTION_NOTIFY_ONE: {
      printf("condition_variable_notify_one\n");
      condition_variable_notify_one(arg[1], arg[2]);
      syscall_return(tf, 0);
    } break;
    case USER_CV_ACTION_NOTIFY_ALL: {
      printf("condition_variable_notify_all\n");
      condition_variable_notify_all(arg[1], arg[2]);
      syscall_return(tf, 0);
    } break;
    default:
      panic("TODO USER ABORT!");
    }
  } break;
  default: {
    printf("unknown system call %d with args:  %d, %d, %d, %d, %d\n", no,
           arg[0], arg[1], arg[2], arg[3], arg[4]);
    abort();
  }
  }
}