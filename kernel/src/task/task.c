#include <assert.h>
#include <compiler_helper.h>
#include <list.h>
#include <panic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>

static list_entry_t tasks;
static struct task *current;
// FIXME atomic
static pid_t id_seq;

static task_t *task_create_impl(bool supervisor) {
  if (current != 0) {
    if (supervisor && !current->supervisor)
      return 0; //只有supervisor才能创造一个supervisor
  }

  task_t *new_task = malloc(sizeof(task_t));
  if (!new_task) {
    return 0;
  }

  list_init(&new_task->list_head);
  new_task->supervisor = supervisor;
  new_task->state = CREATED;

  do {
    new_task->id = id_seq++;
  } while (new_task->id == 0);

  new_task->parent = current;

  if (supervisor) {
    extern uint32_t kernel_page_directory[];
    new_task->pgd = (uintptr_t)(uint32_t *)kernel_page_directory;
  } else {
    //用户任务需要新建一个页表
    panic("ahaaa");
  }

  return new_task;
}

void task_init() {
  //将当前的上下文设置为第一个任务init
  task_t *init = task_create_impl(true);
  if (!init) {
    panic("creating task init failed");
  }
  init->state = RUNNING;
  init->kstack = 0;
  list_init(&tasks);
  list_add(&tasks, &init->list_head);
  current = init;
}

//当前进程
pid_t task_current() {
  if (!current) {
    return 0;
  } else {
    return current->id;
  }
}

#define KSTACK_SIZE (4096)

void kernel_task_entry();

//创建进程
pid_t task_create(void (*func)(void *), void *arg, bool supervisor) {
  task_t *new_task = task_create_impl(supervisor);
  if (!new_task)
    return 0;

  new_task->kstack = (uintptr_t)malloc(KSTACK_SIZE);
  if (!new_task->kstack) {
    goto fail;
  }

  //设置上下文
  memset(&new_task->ctx, 0, sizeof(struct context));
  new_task->ctx.regs.eip = (uint32_t)(uintptr_t)kernel_task_entry;
  new_task->ctx.regs.ebp = new_task->kstack + KSTACK_SIZE;
  new_task->ctx.regs.esp = new_task->kstack + KSTACK_SIZE - 2 * sizeof(void *);
  *(void **)(new_task->ctx.regs.esp + 4) = arg;
  *(void **)(new_task->ctx.regs.esp) = (void *)func;

  list_add(&tasks, &new_task->list_head);
  return new_task->id;

fail:
  if (new_task->kstack) {
    free((void *)new_task->kstack);
  }
  free(new_task);
  return 0;
}

//等待进程结束
void task_join(pid_t pid) { UNUSED(pid); }

//放弃当前进程时间片
void task_yield() {}

//将当前进程挂起一段时间
void task_sleep(uint64_t millisecond) { UNUSED(millisecond); }

//退出当前进程
// aka exit
void task_exit() {
  printf("current task %d exit!\n", current->id);
  panic("task exit!");
}

void switch_to_impl(void *, void *);

//切换到另一个task
void task_switch(pid_t pid) {
  assert(current);
  if (current->id == pid) {
    // FIXME 不应该panic
    panic("task_switch: switch to self is prohibited");
  }
  // FIXME 不应该线性搜索
  task_t *t = 0;
  for (list_entry_t *p = list_next(&tasks); p != &tasks; p = list_next(p)) {
    if (((task_t *)p)->id == pid) {
      t = (task_t *)p;
      break;
    }
  }
  if (t == 0) {
    // FIXME 好像不应该panic
    panic("task_switch: pid not found");
  }
  task_t *prev = current;
  current = t;
  switch_to_impl(&prev->ctx.regs, &t->ctx.regs);
}
