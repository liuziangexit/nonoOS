#include <assert.h>
#include <compiler_helper.h>
#include <list.h>
#include <panic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>

static list_entry_t tasks;
static kernel_task_t *current;
// FIXME atomic
static pid_t id_seq;

static kernel_task_t *task_create_impl(const char *name, bool kernel) {
  if (current != 0) {
    if (kernel && !current->kernel)
      return 0; //只有supervisor才能创造一个supervisor
  }

  kernel_task_t *new_task = malloc(sizeof(kernel_task_t));
  if (!new_task) {
    return 0;
  }

  list_init(&new_task->list_head);
  new_task->kernel = kernel;
  new_task->state = CREATED;

  do {
    new_task->id = id_seq++;
  } while (new_task->id == 0);

  new_task->parent = current;
  new_task->name = name;

  return new_task;
}

#define KSTACK_SIZE (4096)

void kernel_task_entry();

static void task_destory(kernel_task_t *t) {
  assert(t);
  free((void *)t->kstack);
  list_del(&t->list_head);
  free(t);
}

//保存上下文再切换
void switch_to(void *from, void *to);
//直接切换，不保存上下文（exit时用）
void switch_to2(void *to);

// FIXME 不应该线性搜索
static kernel_task_t *task_find(pid_t pid) {
  for (list_entry_t *p = list_next(&tasks); p != &tasks; p = list_next(p)) {
    if (((kernel_task_t *)p)->id == pid) {
      return (kernel_task_t *)p;
    }
  }
  return 0;
}

void task_display() {
  printf("\n\nTask Display   Current: %s", current->name);
  printf("\n****************************\n");
  for (list_entry_t *p = list_next(&tasks); p != &tasks; p = list_next(p)) {
    kernel_task_t *t = (kernel_task_t *)p;
    printf("State:%s  ID:%d  Supervisor:%s  Name:%s\n",
           task_state_str(t->state), (int)t->id, t->kernel ? "T" : "F",
           t->name);
  }
  printf("****************************\n\n");
}

const char *task_state_str(enum task_state s) {
  switch (s) {
  case CREATED:
    return "CREATED";
  case YIELDED:
    return "YIELDED";
  case RUNNING:
    return "RUNNING";
  default:
    panic("zhu ni zhong qiu jie kuai le!");
  }
  __builtin_unreachable();
}

void task_init() {
  //将当前的上下文设置为第一个任务
  kernel_task_t *init = task_create_impl("scheduler", true);
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

//创建进程
pid_t task_create(void (*func)(void *), void *arg, const char *name,
                  bool kernel) {
  kernel_task_t *new_task = task_create_impl(name, kernel);
  if (!new_task)
    return 0;

  new_task->kstack = (uintptr_t)malloc(KSTACK_SIZE);
  if (!new_task->kstack) {
    goto fail;
  }

  //设置上下文
  memset(&new_task->regs, 0, sizeof(struct registers));
  new_task->regs.eip = (uint32_t)(uintptr_t)kernel_task_entry;
  new_task->regs.ebp = new_task->kstack + KSTACK_SIZE;
  new_task->regs.esp = new_task->kstack + KSTACK_SIZE - 2 * sizeof(void *);
  *(void **)(new_task->regs.esp + 4) = arg;
  *(void **)(new_task->regs.esp) = (void *)func;

  list_add(&tasks, &new_task->list_head);
  return new_task->id;

fail:
  task_destory(new_task);
  return 0;
}

//等待进程结束
void task_join(pid_t pid) { UNUSED(pid); }

//放弃当前进程时间片
void task_yield() {}

//将当前进程挂起一段时间
void task_sleep(uint64_t millisecond) { UNUSED(millisecond); }

//退出当前进程
// aka exit()
void task_exit() {
  task_destory(current);
  //找到task schd
  kernel_task_t *schd = task_find(1);
  assert(schd);
  schd->state = RUNNING;
  switch_to2(&schd->regs);
}

//切换到另一个task
void task_switch(pid_t pid) {
  assert(current);
  if (current->id == pid) {
    // FIXME 不应该panic
    panic("task_switch: switch to self is prohibited");
  }

  kernel_task_t *t = task_find(pid);
  if (t == 0) {
    // FIXME 不应该panic
    panic("task_switch: pid not found");
  }
  current->state = YIELDED;
  t->state = RUNNING;
  kernel_task_t *prev = current;
  current = t;
  switch_to(&prev->regs, &t->regs);
}
