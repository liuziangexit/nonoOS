#include <assert.h>
#include <compiler_helper.h>
#include <list.h>
#include <panic.h>
#include <stdint.h>
#include <stdlib.h>
#include <task.h>

static list_entry_t tasks;
static struct task *current;
// FIXME atomic
static pid_t id_seq;

static void switch_to(task_t *t);

static task_t *task_create_impl(bool supervisor, size_t stack_size) {
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
  task_t *init = task_create_impl(true, 0);
  if (!init) {
    panic("creating task init failed");
  }
  init->state = RUNNING;
  init->kstack = 0;
  list_add(&tasks, &init->list_head);
  current = init;

  // for test
  // switch_to(0);
}

//当前进程
pid_t task_current() {
  if (!current) {
    return 0;
  } else {
    return current->id;
  }
}

//创建进程
pid_t task_create(bool supervisor, size_t stack_size) {
  task_t *new_task = task_create_impl(supervisor, stack_size);
  if (!new_task)
    return 0;

  new_task->kstack = (uintptr_t)malloc(4096);
  if (!new_task->kstack) {
    goto fail;
  }
  // TODO: 设置上下文
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
void task_exit() {}

void switch_to_impl(void *, void *);

//切换到另一个task
static void switch_to(task_t *t) {
  assert(current);
  switch_to_impl(&current->ctx.regs, &t->ctx.regs);
}
