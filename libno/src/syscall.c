#ifdef LIBNO_USER
#include "syscall.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define T_SYSCALL 122 // 系统调用中断号
#define MAX_ARGS 5    // 只能支持这么多参数

uint32_t syscall(int call, int cnt, ...) {
  /*
  几个月后完全忘了ring级别的改变和任务切换之间的关系，现在记录在这里

  ring0到ring3的改变是通过interruput_handler文件里的TO_USER那里实现的，也就是处理中断T_SWITCH_USER的地方
  ring3到ring0的改变是通过idt表来实现的，当我们在ring3时，通过触发一个中断，就可以转到ring0（具体逻辑看中断相关的代码，注释还是很清楚的）

  但是当task_switch函数做任务切换时，并没有做ring级别切换，那么这工作是在哪里做的呢？
  A线程->中断->task_switch->中断->B线程
  实际上在task_switch时一定是在内核态的，然后栈上面有一个中断是从用户切到内核的中断。
  当从中断返回时，就自动切换ring级别了，因此不需要task_switch去做这件事
   */

  // 测试代码
  // #ifndef NDEBUG
  uint16_t reg[6];
  uint32_t exx[2];
  asm volatile("mov %%cs, %0;"
               "mov %%ds, %1;"
               "mov %%es, %2;"
               "mov %%fs, %3;"
               "mov %%gs, %4;"
               "mov %%ss, %5;"
               "movl %%esp, %6;"
               "movl %%ebp, %7;"
               : "=m"(reg[0]), "=m"(reg[1]), "=m"(reg[2]), "=m"(reg[3]),
                 "=m"(reg[4]), "=m"(reg[5]), "=m"(exx[0]), "=m"(exx[1]));
  assert((reg[0] & 3) == 3);
  // #endif

  va_list ap;
  va_start(ap, cnt);
  uint32_t a[MAX_ARGS] = {0};
  for (int i = 0; i < cnt && i < MAX_ARGS; i++) {
    a[i] = va_arg(ap, uint32_t);
  }
  va_end(ap);

  uint32_t ret;
  // nop是为了调试
  asm volatile("int %1;nop;nop;nop"
               : "=a"(ret)
               : "i"(T_SYSCALL), "a"(call), "d"(a[0]), "c"(a[1]), "b"(a[2]),
                 "D"(a[3]), "S"(a[4])
               : "cc", "memory");
  return ret;
}
#endif