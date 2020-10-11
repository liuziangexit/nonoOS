#include <assert.h>
#include <compiler_helper.h>
#include <list.h>
#include <panic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>

/*
TODO
这个地方想做一个有序数组，然后查找的方式是二分法，这样我们就可以有logn的查找速度
首先需要实现一个std::vector一样的可变长数组，然后实现二分法的操作（直接实现c标准库里的），就可以了。
*/
static list_entry_t tasks;
static ktask_t *current;
// FIXME atomic
static pid_t id_seq;

static task_group_t *task_group_create_impl(bool is_kernel) {
  task_group_t *group = malloc(sizeof(task_group_t));
  if (!group) {
    return 0;
  }
  list_init(&group->tasks);
  group->task_cnt = 0;
  group->is_kernel = is_kernel;
  group->pgd = 0;
  return group;
}

static void task_group_add(task_group_t *g, ktask_t *t) {
  assert(g);
  list_add(&g->tasks, &t->group_head);
  g->task_cnt++;
  t->group = g;
}

static void task_group_remove(task_group_t *g, ktask_t *t) {
  assert(g);
  if (g->task_cnt == 1) {
    free(g);
  } else {
    g->task_cnt--;
    list_del(&t->group_head);
  }
  list_init(&t->group_head);
  t->group = 0;
}

//因为ktask里面那个group
// head不是第一个成员，所以需要加减一下指针才能得到ktask的地址
inline static ktask_t *task_group_list_retrieve(list_entry_t *head) {
  return (ktask_t *)(head - 1);
}

//不设置
static ktask_t *task_create_impl(const char *name, bool kernel,
                                 task_group_t *group) {
  //检查kernel参数是否合法
  if (current && kernel && !current->group->is_kernel) {
    return 0; //只有supervisor才能创造一个supervisor
  }
  if (group && (kernel != group->is_kernel)) {
    return 0;
  }

  if (!group) {
    //创建一个group
    group = task_group_create_impl(kernel);
    if (!group) {
      return 0;
    }
  }

  ktask_t *new_task = malloc(sizeof(ktask_t));
  if (!new_task) {
    return 0;
  }

  list_init(&new_task->global_head);
  list_init(&new_task->group_head);
  new_task->state = CREATED;

  //生成pid
  do {
    new_task->id = id_seq++;
  } while (new_task->id == 0);

  new_task->parent = current;
  new_task->name = name;

  //加入group
  task_group_add(group, new_task);

  return new_task;
}

#define KSTACK_SIZE (4096)

void kernel_task_entry();

static void task_destory(ktask_t *t) {
  assert(t);
  list_del(&t->global_head);
  free((void *)t->kstack);
  task_group_remove(t->group, t);
  free(t);
}

//保存上下文再切换
void switch_to(void *from, void *to);
//直接切换，不保存上下文（exit时用）
void switch_to2(void *to);

// FIXME 不应该线性搜索
ktask_t *task_find(pid_t pid) {
  for (list_entry_t *p = list_next(&tasks); p != &tasks; p = list_next(p)) {
    if (((ktask_t *)p)->id == pid) {
      return (ktask_t *)p;
    }
  }
  return 0;
}

void task_display() {
  printf("\n\nTask Display   Current: %s", current->name);
  printf("\n****************************\n");
  for (list_entry_t *p = list_next(&tasks); p != &tasks; p = list_next(p)) {
    ktask_t *t = (ktask_t *)p;
    printf("State:%s  ID:%d  Supervisor:%s  Group:%d  Name:%s\n",
           task_state_str(t->state), (int)t->id,
           t->group->is_kernel ? "T" : "F", (int32_t)t->group, t->name);
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
  ktask_t *init = task_create_impl("idle", true, 0);
  if (!init) {
    panic("creating task init failed");
  }
  init->state = RUNNING;
  init->kstack = 0;
  list_init(&tasks);
  list_add(&tasks, &init->global_head);
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
//如果group为0，说明要为这个新task新建一个group。否则把这个新task加到指定的group里
pid_t task_create(void (*func)(void *), void *arg, const char *name,
                  bool kernel, task_group_t *group) {
  ktask_t *new_task = task_create_impl(name, kernel, group);
  if (!new_task)
    return 0;

  new_task->kstack = (uintptr_t)malloc(KSTACK_SIZE);
  if (!new_task->kstack) {
    task_destory(new_task);
    return 0;
  }

  //设置上下文和内核栈
  memset(&new_task->regs, 0, sizeof(struct registers));
  new_task->regs.eip = (uint32_t)(uintptr_t)kernel_task_entry;
  new_task->regs.ebp = new_task->kstack + KSTACK_SIZE;
  new_task->regs.esp = new_task->kstack + KSTACK_SIZE - 2 * sizeof(void *);
  *(void **)(new_task->regs.esp + 4) = arg;
  *(void **)(new_task->regs.esp) = (void *)func;

  list_add(&tasks, &new_task->global_head);
  return new_task->id;
}

//等待进程结束
void task_join(pid_t pid) {
  UNUSED(pid);
  panic("not implemented");
}

//放弃当前进程时间片
void task_yield() { panic("not implemented"); }

//将当前进程挂起一段时间
void task_sleep(uint64_t millisecond) {
  UNUSED(millisecond);
  panic("not implemented");
}

//退出当前进程
// aka exit()
/*
FIXME
这里现在还有问题，当前线程的栈已经被回收了，但是代码却还在使用它
解决这个问题的方法是，增加一个“已退出”的状态，在这个地方把当前线程
设置为已退出，然后直接切到别的线程，由别的线程来destory当前线程的资源（主要是栈）

至于这个“别的线程”是不是idle，再讨论

由于现在还没有抢占，所以这个问题不会被暴露出来
*/
void task_exit() {
  task_destory(current);
  //找到idle task，它的pid是1
  ktask_t *schd = task_find(1);
  assert(schd);
  schd->state = RUNNING;
  switch_to2(&schd->regs);
}

//切换到另一个task
// TODO 这个是不是应该限制只有内核态才能用？
void task_switch(pid_t pid) {
  assert(current);
  if (current->id == pid) {
    // FIXME 不应该panic
    panic("task_switch: switch to self is prohibited");
  }

  ktask_t *t = task_find(pid);
  if (t == 0) {
    // FIXME 不应该panic
    panic("task_switch: pid not found");
  }
  current->state = YIELDED;
  t->state = RUNNING;
  ktask_t *prev = current;
  current = t;
  switch_to(&prev->regs, &t->regs);
}
