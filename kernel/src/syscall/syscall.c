#include "../../include/syscall.h"
#include "../../../libno/include/syscall.h"
#include "debug.h"
#include <stdio.h>
#include <task.h>
#include <virtual_memory.h>
#include <x86.h>

void syscall_return(struct trapframe *tf, uint32_t ret) {
  tf->tf_gprs.reg_eax = ret;
}

void syscall_dispatch(struct trapframe *tf) {
  uint32_t arg[5] = {tf->tf_gprs.reg_edx, tf->tf_gprs.reg_ecx,
                     tf->tf_gprs.reg_ebx, tf->tf_gprs.reg_edi,
                     tf->tf_gprs.reg_esi};
  int no = tf->tf_gprs.reg_eax;

  switch (no) {
  case SYSCALL_EXIT: {
    printf("exit() with args: %d, %d, %d, %d, %d\n", arg[0], arg[1], arg[2],
           arg[3], arg[4]);
    task_exit();
  } break;
  case SYSCALL_ALLOC: {
    printf("aligned_alloc() with args: %d, %d, %d, %d, %d\n", arg[0], arg[1],
           arg[2], arg[3], arg[4]);
    ktask_t *task = task_find(task_current());
    assert(task && !task->group->is_kernel);
    // arg[0]也就是alignment不需要用到，因为umalloc直接是平台最大对齐的
    uintptr_t vaddr = umalloc(task->group->vm, arg[1], true, 0, 0);
    printf("aligned_alloc() returned: 0x%09llx\n", (int64_t)vaddr);
    syscall_return(tf, vaddr);
  } break;
  case SYSCALL_FREE: {
    printf("free() with args: %d, %d, %d, %d, %d\n", arg[0], arg[1], arg[2],
           arg[3], arg[4]);
    ktask_t *task = task_find(task_current());
    assert(task && !task->group->is_kernel);
    ufree(task->group->vm, arg[0]);
    syscall_return(tf, 0);
  } break;
  case SYSCALL_GETPID: {
    printf("getpid() with args: %d, %d, %d, %d, %d\n", arg[0], arg[1], arg[2],
           arg[3], arg[4]);
    printf("\n\n");
    print_cur_status();
    printf("\n\nhlt()");
    hlt();
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