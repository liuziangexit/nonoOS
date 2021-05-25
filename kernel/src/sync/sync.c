#include <mmu.h>
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
}

void leave_critical_region(uint32_t *save) {
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
}
// 如果此前关闭了中断，开启中断
void leave_noint_region(uint32_t *save) {
  if (*save) {
    enable_interrupt();
  }
}