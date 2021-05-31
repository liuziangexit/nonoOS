#include "task.h"
#include <compiler_helper.h>
#include <defs.h>
#include <mmu.h>
#include <panic.h>
#include <x86.h>

static __always_inline void disable_interrupt() { cli(); }
static __always_inline void enable_interrupt() { sti(); }
static __always_inline void make_sure_schd_disabled() {
  if (task_preemptive_enabled()) {
    panic("make_sure_schd_disabled failed");
  }
}
static __always_inline void make_sure_int_disabled() {
  if (reflags() & FL_IF) {
    panic("make_sure_int_disabled failed");
  }
}

// 如果当前已经启用抢占，关闭抢占
void enter_critical_region(uint32_t *save);
// 如果此前关闭了抢占，开启抢占
void leave_critical_region(uint32_t *save);
// 如果当前已经启用中断，关闭中断
void enter_noint_region(uint32_t *save);
// 如果此前关闭了中断，开启中断
void leave_noint_region(uint32_t *save);

// SMART_CRITICAL_REGION宏在当前scope中关闭抢占，离开当前scope时开始抢占
#ifdef __GNUC__
#define SMART_CRITICAL_REGION                                                  \
  uint32_t __smart_critical_region__                                           \
      __attribute__((cleanup(leave_critical_region)));                         \
  enter_critical_region(&__smart_critical_region__);
#endif

#ifdef __GNUC__
#define SMART_NOINT_REGION                                                     \
  uint32_t __smart_noint_region__                                              \
      __attribute__((cleanup(leave_noint_region)));                            \
  enter_noint_region(&__smart_noint_region__);
#endif

// 参考C11线程支持库
// 可重入锁
struct mutex {};
uint32_t mutex_create();
void mutex_destroy(uint32_t mut_id);
bool mutex_trylock(uint32_t mut_id);
void mutex_lock(uint32_t mut_id);
bool mutex_timedlock(uint32_t mut_id, uint32_t timeout_ms);
void mutex_unlock(uint32_t mut_id);
// 条件变量
/*
在POSIX系统里，条件变量要配合mutex一起使用的原因是
1)
2)
但是由于
1)
2)
所以nonoOS的条件变量可以不与mutex一起使用
*/
struct condition_variable {};
uint32_t condition_variable_create();
void condition_variable_destroy(uint32_t cv_id);
void condition_variable_wait(uint32_t cv_id, uint32_t mut_id);
bool condition_variable_timedwait(uint32_t cv_id, uint32_t mut_id,
                                  uint32_t timeout_ms);
void condition_variable_notify_one(uint32_t cv_id);
void condition_variable_notify_all(uint32_t cv_id);
