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
static list_entry_t tasks_exited;
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
}

//因为ktask里面那个group
// head不是第一个成员，所以需要加减一下指针才能得到ktask的地址
inline static ktask_t *task_group_list_retrieve(list_entry_t *head) {
  return (ktask_t *)(head - 1);
}

static ktask_t *task_create_impl(const char *name, bool kernel,
                                 task_group_t *group) {
  //检查参数是否合法
  if (current && kernel && !current->group->is_kernel) {
    return 0; //只有supervisor才能创造一个supervisor
  }
  if (group && (kernel != group->is_kernel)) {
    return 0;
  }
  if (kernel) {
    //如果是内核级
    if (group) {
      //如果指定了线程组，那么必须是idle所在的那个组
      if (task_find(1)->group != group) {
        return 0;
      }
    } else {
      //如果没有指定线程组（意思是要创建一个），那么确保这是首个内核线程（idle）
      if (task_find(1))
        return 0;
    }
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
  /* FIXME
   1.当系统中存在的进程数量达到2^32-1时（正在创建第2^32个进程时），这会无限循环
   2.当以特别快的速度创建和销毁进程时，比如说线程1在此处创建了任务1，但是还未将任务1
  加入进程列表时，另一个线程快速地用光了所有剩下的id，以至于id_seq从1重新开始，
  这个时候就是一个racing了

  对于第一个问题，下面已经作出了处理，但是可以看出还不够充分，需要对下面的代码加锁才是真正正确的
  对于第二个问题，加一个锁就可以解决

  所以这个地方目前只需要加锁就完事了，等锁做好了记得来改
  */
  const pid_t begins = id_seq;
  while (new_task->id = ++id_seq && task_find(new_task->id)) {
    if (new_task->id == begins)
      return 0; // running out of pid
  }

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
  free((void *)t->kstack);
  task_group_remove(t->group, t);
#ifndef NDEBUG
  printf("task_destory: destroying task %s\n", t->name);
#endif
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
  case EXITED:
    return "EXITED";
  default:
    panic("zhu ni zhong qiu jie kuai le!");
  }
  __builtin_unreachable();
}

//这个由idle线程执行，每次idle被调度的时候，都要执行这个函数
//要限制这个的执行权限
void destroy_exited() {
  if (!list_empty(&tasks_exited)) {
    for (list_entry_t *p = list_next(&tasks_exited); p != &tasks_exited;) {
      list_entry_t *next = list_next(p);
      task_destory((ktask_t *)p);
      p = next;
    }
    list_init(&tasks_exited);
  }
}

void task_init() {
  list_init(&tasks);
  list_init(&tasks_exited);
  //将当前的上下文设置为第一个任务
  ktask_t *init = task_create_impl("scheduler", true, 0);
  if (!init) {
    panic("creating task init failed");
  }
  init->state = RUNNING;
  init->kstack = 0;
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
void task_exit() {
  //把current设置为EXITED并且加入到tasks_exited链表中，之后schd线程会对它调用destory的
  current->state = EXITED;
  list_del(&current->global_head);
  list_init(&current->global_head);
  list_add(&tasks_exited, &current->global_head);
  //找到schd task，它的pid是1
  ktask_t *schd = task_find(1);
  assert(schd);
  //切换到schd
  schd->state = RUNNING;
  current = schd;
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
