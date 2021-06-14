#include "task.h"
#include <compiler_helper.h>
#include <defs.h>
#include <mmu.h>
#include <panic.h>
#include <task.h>
#include <vector.h>
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

// 使用下面的内核对象时，需要自己进行引用

// 参考C11线程支持库
// TODO 做成可重入锁
struct mutex {
  uint32_t obj_id;  // 内核对象id
  uint32_t ref_cnt; // 引用此对象的线程数量
  uint32_t locked;  // 当前是否已锁
  pid_t owner;      // 拥有者
  vector_t waitors; // 等待者
};
typedef struct mutex mutex_t;
pid_t mutex_owner(uint32_t mut_id);
uint32_t mutex_create();
void mutex_destroy(mutex_t *);
bool mutex_trylock(uint32_t mut_id);
void mutex_lock(uint32_t mut_id);
bool mutex_timedlock(uint32_t mut_id, uint32_t timeout_ms);
void mutex_unlock(uint32_t mut_id);

// 条件变量
// 为什么notify不传入一个mutex，也就是强制先notify再unlock，来保证绝对不会丢通知呢？因为这是以性能为代价的，比如有的实现下，wait那边会有一次假唤醒。因此，将这个决定给程序员做
// 考虑在我的线程池worker里，以及task_join里，能不能不需要锁？稍有常识的人都能看出，不能。我只想说懂的都懂，我也不想解释了
// Java
// Object里的notify/wait为什么不需要锁？这是一个假命题，看一下文档就知道，他们也需要锁
struct condition_variable {
  uint32_t obj_id;  // 内核对象id
  uint32_t ref_cnt; // 引用此对象的线程数量
  vector_t waitors; // 等待者
};
typedef struct condition_variable condition_variable_t;
uint32_t condition_variable_create();
void condition_variable_destroy(condition_variable_t *cv);
void condition_variable_wait(uint32_t cv_id, uint32_t mut_id);
bool condition_variable_timedwait(uint32_t cv_id, uint32_t mut_id,
                                  uint64_t timeout_ms);
void condition_variable_notify_one(uint32_t cv_id, uint32_t mut_id);
void condition_variable_notify_all(uint32_t cv_id, uint32_t mut_id);
