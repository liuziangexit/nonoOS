#include <assert.h>
#include <atomic.h>
#include <kernel_object.h>
#include <memory_barrier.h>
#include <mmu.h>
#include <stdlib.h>
#include <sync.h>
#include <task.h>
#include <x86.h>

// TODO
// 单核多任务环境下，需不需要内存屏障？
// 从逻辑上就可以推出，是不需要的。如果需要的话，每次上下文切换不都要加屏障吗，但大家切上下文的时候都不这样做
// 但这个问题还要进一步研究，morespecifically，乱序流水线和分支预测等东西的存在什么时候会被程序观测到（我猜答案是多核心时候会被观测到）？
// 但是为什么呢？这是一个学术问题，需要进一步研究

void enter_critical_region(uint32_t *save) {
  if (task_preemptive_enabled()) {
    task_preemptive_set(false);
    *save = 1;
  } else {
    *save = 0;
  }
  memory_barrier(SEQ_CST);
}

void leave_critical_region(uint32_t *save) {
  memory_barrier(SEQ_CST);
  if (*save) {
    task_preemptive_set(true);
  }
}

// 如果当前已经启用中断，关闭中断
void enter_noint_region(uint32_t *save) {
  if (reflags() & FL_IF) {
    disable_interrupt();
    *save = 1;
  } else {
    *save = 0;
  }
  memory_barrier(SEQ_CST);
}
// 如果此前关闭了中断，开启中断
void leave_noint_region(uint32_t *save) {
  memory_barrier(SEQ_CST);
  if (*save) {
    enable_interrupt();
  }
}

uint32_t mutex_create() {
  mutex_t *mut = malloc(sizeof(mutex_t));
  assert(mut);
  mut->obj_id = kernel_object_new(KERNEL_OBJECT_MUTEX, mut);
  mut->locked = 0;
  mut->owner = 0;
  mut->ref_cnt = 0;
  return mut->obj_id;
}

void mutex_destroy(mutex_t *mut) {
  if (mut->ref_cnt != 0 || mut->locked != 0 || mut->owner != 0)
    task_terminate(TASK_TERMINATE_ABORT);
  free(mut);
}

bool mutex_trylock(uint32_t mut_id) {
  mutex_t *mut = kernel_object_get(mut_id);
  if (!mut)
    task_terminate(TASK_TERMINATE_MUT_NOT_FOUND);
  if (!atomic_compare_exchange(&mut->locked, 0, 1)) {
    return false;
  }
  atomic_store(&mut->owner, task_current()->id);
  return true;
}

void mutex_lock(uint32_t mut_id);

bool mutex_timedlock(uint32_t mut_id, uint32_t timeout_ms);

void mutex_unlock(uint32_t mut_id) {
  mutex_t *mut = kernel_object_get(mut_id);
  if (!mut)
    task_terminate(TASK_TERMINATE_MUT_NOT_FOUND);
  uint32_t owner = atomic_load(&mut->owner);
  if (owner != task_current()->id) {
    task_terminate(TASK_TERMINATE_ABORT);
  }
  atomic_store(&mut->owner, 0);
  atomic_store(&mut->locked, 0);
}
