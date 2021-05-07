#include <mmu.h>
#include <sync.h>
#include <x86.h>

void disable_interrupt() { cli(); }
void enable_interrupt() { sti(); }
void make_sure_int_disabled() {
  if ((reflags() & FL_IF) != 0) {
    panic("make_sure_int_disabled failed");
  }
}

//如果当前已经启用中断，关闭中断
void enter_critical_region(uint32_t *save) {
  if (reflags() & FL_IF) {
    disable_interrupt();
    *save = 1;
  } else {
    *save = 0;
  }
}

//如果此前关闭了中断，开启中断
void leave_critical_region(uint32_t *save) {
  if (*save) {
    enable_interrupt();
  }
}