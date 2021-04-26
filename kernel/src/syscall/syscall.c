#include "../../include/syscall.h"
#include <stdio.h>
void syscall(struct trapframe *tf) {
  uint32_t arg[5];
  int num = tf->tf_gprs.reg_eax;
  printf("systeam call %d with args: %d, %d, %d, %d, %d\n", num,
         tf->tf_gprs.reg_edx, tf->tf_gprs.reg_ecx, tf->tf_gprs.reg_ebx,
         tf->tf_gprs.reg_edi, tf->tf_gprs.reg_esi);
  tf->tf_gprs.reg_eax = 1;
}