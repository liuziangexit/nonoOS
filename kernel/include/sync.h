#include <defs.h>
#include <panic.h>

void disable_interrupt();
void enable_interrupt();
void make_sure_int_disabled();

// 如果当前已经启用中断，关闭中断
void enter_critical_region(uint32_t *save);
// 如果此前关闭了中断，开启中断
void leave_critical_region(uint32_t *save);

// SMART_CRITICAL_REGION宏在当前scope中关闭中断，离开当前scope时开启中断
// FIXME FIXME FIXME FIXME FIXME FIXME
// 当多个栈帧使用SMART_CRITICAL_REGION时，首个leave_critical_region可能会错误地关闭中断
#ifdef __GNUC__
#define SMART_CRITICAL_REGION                                                  \
  uint32_t __smart_critical_region__                                           \
      __attribute__((cleanup(leave_critical_region)));                         \
  enter_critical_region(&__smart_critical_region__);
#endif
