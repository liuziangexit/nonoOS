#include <defs.h>
#include <panic.h>

void disable_interrupt();
void enable_interrupt();
void make_sure_int_disabled();

//如果当前已经启用中断，关闭中断
void enter_critical_region(uint32_t *save);
//如果此前关闭了中断，开启中断
void leave_critical_region(uint32_t *save);

#define SMART_CRITICAL_REGION                                                  \
  uint32_t __smart_critical_region__                                           \
      __attribute__((cleanup(leave_critical_region)));                         \
  enter_critical_region(&__smart_critical_region__);
