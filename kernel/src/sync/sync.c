#include <assert.h>
#include <atomic.h>
#include <clock.h>
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

pid_t mutex_owner(uint32_t mut_id) {
  SMART_CRITICAL_REGION
  mutex_t *mut = kernel_object_get(mut_id);
  return atomic_load(&mut->owner);
}

uint32_t mutex_create() {
  mutex_t *mut = malloc(sizeof(mutex_t));
  assert(mut);
  mut->locked = 0;
  mut->owner = 0;
  mut->ref_cnt = 0;
  vector_init(&mut->waitors, sizeof(pid_t), NULL);
  mut->obj_id = kernel_object_new(KERNEL_OBJECT_MUTEX, mut);
  return mut->obj_id;
}

bool mutex_destroy(mutex_t *mut) {
  // 这个检查已经在kernel_object框架里做了
  // if (mut->ref_cnt != 0) {
  //   panic("mut->ref_cnt != 0");
  // }
  if (mut->locked != 0 || mut->owner != 0 || vector_count(&mut->waitors) != 0) {
    // 没有人引用这个内核对象，但是却有人在等待这个mutex，让一个等待者引用该对象，取消destroy
    terminal_fgcolor(CGA_COLOR_RED);
    printf("there are still tasks waiting on this mutex\n");
    terminal_default_color();
    bool succ = kernel_object_ref_safe(*(pid_t *)vector_get(&mut->waitors, 0),
                                       mut->obj_id);
    assert(succ);
    return false;
  }
  vector_destroy(&mut->waitors);
  free(mut);
  return true;
}

bool mutex_trylock(uint32_t mut_id) {
  SMART_CRITICAL_REGION
  mutex_t *mut = kernel_object_get(mut_id);
  if (!mut)
    task_terminate(TASK_TERMINATE_INVALID_ARGUMENT);
  uint32_t expected = 0;
  if (!atomic_compare_exchange(&mut->locked, &expected, 1)) {
    // 失败了
    if (atomic_load(&mut->owner) == task_current()->id) {
      terminal_fgcolor(CGA_COLOR_RED);
      printf("mutex_trylock logic error\n");
      terminal_default_color();
      task_terminate(TASK_TERMINATE_ABORT);
    }
    return false;
  }
  atomic_store(&mut->owner, task_current()->id);
  return true;
}

void mutex_lock(uint32_t mut_id) {
  SMART_CRITICAL_REGION
  mutex_t *mut = kernel_object_get(mut_id);
  if (!mut)
    task_terminate(TASK_TERMINATE_INVALID_ARGUMENT);
  uint32_t expected = 0;
  if (!atomic_compare_exchange(&mut->locked, &expected, 1)) {
    // 失败了
    SMART_NOINT_REGION
    if (atomic_load(&mut->owner) == task_current()->id) {
      terminal_fgcolor(CGA_COLOR_RED);
      printf("mutex_lock logic error\n");
      terminal_default_color();
      task_terminate(TASK_TERMINATE_ABORT);
    }
    task_current()->tslice++;
    task_current()->wait_type = WAIT_MUTEX;
    task_current()->wait_ctx.mutex.after = 0; // 设置为0，表示没有超时
    task_current()->wait_ctx.mutex.timeout = false;
    // 添加进等待此mutex的列表
    uint32_t idx = vector_add(&mut->waitors, &task_current()->id);
    // 陷入等待
    task_schd(true, true, WAITING);
    // 从等待列表中移除我自己
    vector_remove(&mut->waitors, idx);
    // 获得锁
    expected = 0;
    if (!atomic_compare_exchange(&mut->locked, &expected, 1)) {
      terminal_fgcolor(CGA_COLOR_RED);
      printf("mutex_lock logic error 2\n");
      terminal_default_color();
      task_terminate(TASK_TERMINATE_ABORT);
    }
  }
  atomic_store(&mut->owner, task_current()->id);
}

bool mutex_timedlock(uint32_t mut_id, uint32_t timeout_ms) {
  SMART_CRITICAL_REGION
  mutex_t *mut = kernel_object_get(mut_id);
  if (!mut)
    task_terminate(TASK_TERMINATE_INVALID_ARGUMENT);
  uint32_t expected = 0;
  if (!atomic_compare_exchange(&mut->locked, &expected, 1)) {
    // 失败了
    SMART_NOINT_REGION
    if (atomic_load(&mut->owner) == task_current()->id) {
      terminal_fgcolor(CGA_COLOR_RED);
      printf("mutex_timedlock logic error\n");
      terminal_default_color();
      task_terminate(TASK_TERMINATE_ABORT);
    }
    task_current()->tslice++;
    task_current()->wait_type = WAIT_MUTEX;
    task_current()->wait_ctx.mutex.after =
        clock_get_ticks() * TICK_TIME_MS + timeout_ms;
    task_current()->wait_ctx.mutex.timeout = false;
    // 添加进等待此mutex的列表
    uint32_t idx = vector_add(&mut->waitors, &task_current()->id);
    // 陷入等待
    task_schd(true, true, WAITING);
    // 从等待列表中移除我自己
    vector_remove(&mut->waitors, idx);
    if (task_current()->wait_ctx.mutex.timeout) {
      // 由超时导致返回
      return false;
    }
    // 获得锁
    expected = 0;
    if (!atomic_compare_exchange(&mut->locked, &expected, 1)) {
      terminal_fgcolor(CGA_COLOR_RED);
      printf("mutex_timedlock logic error 2\n");
      terminal_default_color();
      task_terminate(TASK_TERMINATE_ABORT);
    }
  }
  atomic_store(&mut->owner, task_current()->id);
  return true;
}

void mutex_unlock(uint32_t mut_id) {
  SMART_CRITICAL_REGION
  mutex_t *mut = kernel_object_get(mut_id);
  if (!mut)
    task_terminate(TASK_TERMINATE_INVALID_ARGUMENT);
  uint32_t owner = atomic_load(&mut->owner);
  // 首先验证owner是不是我
  if (owner != task_current()->id) {
    terminal_fgcolor(CGA_COLOR_RED);
    printf("mutex_unlock logic error\n");
    terminal_default_color();
    task_terminate(TASK_TERMINATE_ABORT);
  }
  // 设置owner为0
  atomic_store(&mut->owner, 0);
  // 找第一个等待队列里的线程出来
  // 如果没有也没关系
  if (vector_count(&mut->waitors) != 0) {
    ktask_t *t = task_find(*(pid_t *)vector_get(&mut->waitors, 0));
    t->state = YIELDED;
    ready_queue_put(t);
  }
  atomic_store(&mut->locked, 0);
}

void enter_smart_lock(uint32_t *mut_id) { mutex_lock(*mut_id); }
void leave_smart_lock(uint32_t *mut_id) { mutex_unlock(*mut_id); }

uint32_t condition_variable_create() {
  condition_variable_t *cv = malloc(sizeof(condition_variable_t));
  assert(cv);
  cv->ref_cnt = 0;
  vector_init(&cv->waitors, sizeof(pid_t), NULL);
  cv->obj_id = kernel_object_new(KERNEL_OBJECT_CONDITION_VARIABLE, cv);
  return cv->obj_id;
}

bool condition_variable_destroy(condition_variable_t *cv) {
  // 这个检查已经在kernel_object框架里做了
  // if (cv->ref_cnt != 0) {
  //   panic("cv->ref_cnt != 0");
  // }
  if (vector_count(&cv->waitors) != 0) {
    // 没有人引用这个内核对象，但是却有人在等待这个cv，让一个等待者引用该对象，取消destroy
    terminal_fgcolor(CGA_COLOR_RED);
    printf("there are still tasks waiting on this cv\n");
    terminal_default_color();
    bool succ = kernel_object_ref_safe(*(pid_t *)vector_get(&cv->waitors, 0),
                                       cv->obj_id);
    assert(succ);
    return false;
  }
  vector_destroy(&cv->waitors);
  free(cv);
  return true;
}

void condition_variable_wait(uint32_t cv_id, uint32_t mut_id) {
  if (mutex_owner(mut_id) != task_current()->id) {
    terminal_fgcolor(CGA_COLOR_RED);
    printf("condition_variable_wait logic error\n");
    terminal_default_color();
    task_terminate(TASK_TERMINATE_ABORT);
  }
  condition_variable_t *cv = kernel_object_get(cv_id);
  vector_add(&cv->waitors, &task_current()->id);
  // 关中断是为了确保此线程陷入等待对于另一个线程的notify具有happens-before
  SMART_CRITICAL_REGION
  mutex_unlock(mut_id);
  task_current()->tslice++;
  task_current()->wait_type = WAIT_CV;
  task_current()->wait_ctx.mutex.after = 0;
  task_current()->wait_ctx.mutex.timeout = false;
  // 陷入等待
  task_schd(true, true, WAITING);
  assert(!task_current()->wait_ctx.cv.timeout);
  // 被唤醒了
  // 这里不用try_lock而是lock是因为当notify_all时，某个try_lock可能失败，从而导致逻辑错误
  mutex_lock(mut_id);
}

bool condition_variable_timedwait(uint32_t cv_id, uint32_t mut_id,
                                  uint64_t timeout_ms) {
  if (mutex_owner(mut_id) != task_current()->id) {
    terminal_fgcolor(CGA_COLOR_RED);
    printf("condition_variable_timedwait logic error\n");
    terminal_default_color();
    task_terminate(TASK_TERMINATE_ABORT);
  }
  condition_variable_t *cv = kernel_object_get(cv_id);
  vector_add(&cv->waitors, &task_current()->id);
  // 关中断是为了确保此线程陷入等待对于另一个线程的notify具有happens-before
  SMART_CRITICAL_REGION
  mutex_unlock(mut_id);
  task_current()->tslice++;
  task_current()->wait_type = WAIT_CV;
  task_current()->wait_ctx.mutex.after =
      clock_get_ticks() * TICK_TIME_MS + timeout_ms;
  task_current()->wait_ctx.mutex.timeout = false;
  // 陷入等待
  task_schd(true, true, WAITING);
  if (task_current()->wait_ctx.cv.timeout) {
    return false;
  }
  // 被唤醒了
  // 这里不用try_lock而是lock是因为当notify_all时，某个try_lock可能失败，从而导致逻辑错误
  mutex_lock(mut_id);
  return true;
}

void condition_variable_notify_one(uint32_t cv_id, uint32_t mut_id) {
  mutex_lock(mut_id);
  condition_variable_t *cv = kernel_object_get(cv_id);
  if (vector_count(&cv->waitors) != 0) {
    SMART_CRITICAL_REGION
    ktask_t *t = task_find(*(pid_t *)vector_get(&cv->waitors, 0));
    t->state = YIELDED;
    ready_queue_put(t);
    vector_remove(&cv->waitors, 0);
  }
  mutex_unlock(mut_id);
}

void condition_variable_notify_all(uint32_t cv_id, uint32_t mut_id) {
  mutex_lock(mut_id);
  condition_variable_t *cv = kernel_object_get(cv_id);
  if (vector_count(&cv->waitors) != 0) {
    SMART_CRITICAL_REGION
    for (uint32_t i = 0; i < vector_count(&cv->waitors); i++) {
      ktask_t *t = task_find(*(pid_t *)vector_get(&cv->waitors, i));
      t->state = YIELDED;
      ready_queue_put(t);
    }
    vector_clear(&cv->waitors);
  }
  mutex_unlock(mut_id);
}
