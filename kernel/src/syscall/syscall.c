#include "../../include/syscall.h"
#include "../../../libno/include/syscall.h"
#include "debug.h"
#include <panic.h>
#include <shared_memory.h>
#include <stdio.h>
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
  case SYSCALL_EXIT: {
#ifdef VERBOSE
    terminal_fgcolor(CGA_COLOR_LIGHT_YELLOW);
    printf("exit() with args: %d, %d, %d, %d, %d\n", arg[0], arg[1], arg[2],
           arg[3], arg[4]);
    terminal_default_color();
#endif
    task_exit(arg[0]);
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
  case SYSCALL_GETPID: {
    syscall_return(tf, task->id);
  } break;
  case SYSCALL_PRINTF: {
    int ret = printf_impl((void *)arg[0], (void *)arg[1]);
    syscall_return(tf, ret);
  } break;
  case SYSCALL_SLEEP: {
    uint64_t ms = arg[0];
    ms <<= 32;
    ms |= arg[1];
    task_sleep(ms);
    syscall_return(tf, 0);
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
  default: {
    printf("unknown systeam call %d with args:  %d, %d, %d, %d, %d\n", no,
           arg[0], arg[1], arg[2], arg[3], arg[4]);
    printf("\n\n");
    print_cur_status();
    printf("\n\nhlt()");
    hlt();
  }
  }
}