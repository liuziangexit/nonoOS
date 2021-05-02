#ifdef LIBNO_USER
#include "syscall.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define T_SYSCALL 122 //系统调用中断号
#define MAX_ARGS 5

int32_t syscall(int call, int cnt, ...) {
  va_list ap;
  va_start(ap, cnt);
  uint32_t a[MAX_ARGS] = {0};
  for (int i = 0; i < cnt && i < MAX_ARGS; i++) {
    a[i] = va_arg(ap, uint32_t);
  }
  va_end(ap);

  int ret;
  asm volatile("int %1;"
               : "=a"(ret)
               : "i"(T_SYSCALL), "a"(call), "d"(a[0]), "c"(a[1]), "b"(a[2]),
                 "D"(a[3]), "S"(a[4])
               : "cc", "memory");
  return ret;
}
#endif