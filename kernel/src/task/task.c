#include <compiler_helper.h>
#include <list.h>
#include <panic.h>
#include <stdlib.h>
#include <task.h>

static list_entry_t tasks;
static struct task *current;
// FIXME atomic
static pid_t id_seq;

void task_init() {}

//当前进程
pid_t task_current() {
  if (!current) {
    return 0;
  } else {
    return current->id;
  }
}

static task_t *task_create_impl(bool supervisor, size_t stack_size) {
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
    //内核任务不需要用户态的栈
    new_task->ustack = 0;
  } else {
    //用户任务需要开辟一个栈
    new_task->ustack = (uintptr_t)malloc(stack_size);
    if (!new_task->ustack) {
      goto free_task;
    }
  }

  new_task->kstack = (uintptr_t)malloc(4096);
  if (!new_task->kstack) {
    goto destruct_task;
  }

  if (supervisor) {
    extern uint32_t kernel_page_directory[];
    new_task->pgd = (uintptr_t)(uint32_t *)kernel_page_directory;
  } else {
    //用户任务需要新建一个页表
    panic("ahaaa");
  }

  return new_task;

destruct_task:
  if (new_task->ustack) {
    free((void *)new_task->ustack);
  }
  if (new_task->kstack) {
    free((void *)new_task->kstack);
  }
free_task:
  free(new_task);
  return 0;
}

//创建进程
pid_t task_create(bool supervisor, size_t stack_size) {
  task_t *new_task = task_create_impl(supervisor, stack_size);
  if (!new_task)
    return 0;
  // TODO: 设置上下文
  list_add(&tasks, &new_task->list_head);
  return new_task->id;
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
