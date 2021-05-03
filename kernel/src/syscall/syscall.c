#include "../../include/syscall.h"
#include "../../../libno/include/syscall.h"
#include "debug.h"
#include <stdio.h>
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
    printf("\n\n");
    print_cur_status();
    printf("\n\nhlt()");
    hlt();
  } break;
  case SYSCALL_ALLOC: {
    printf("aligned_alloc() with args: %d, %d, %d, %d, %d\n", arg[0], arg[1],
           arg[2], arg[3], arg[4]);
    syscall_return(tf, 9710);
  } break;
  case SYSCALL_FREE: {
    printf("free() with args: %d, %d, %d, %d, %d\n", arg[0], arg[1], arg[2],
           arg[3], arg[4]);
    printf("\n\n");
    print_cur_status();
    printf("\n\nhlt()");
    hlt();
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