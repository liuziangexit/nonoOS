#include <assert.h>
#include <compiler_helper.h>
#include <elf.h>
#include <gdt.h>
#include <list.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <panic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <x86.h>

/*
TODO
这个地方想做一个有序数组，然后查找的方式是二分法，这样我们就可以有logn的查找速度
首先需要实现一个std::vector一样的可变长数组，然后实现二分法的操作（直接实现c标准库里的），就可以了。
*/
static list_entry_t tasks;
static list_entry_t dead_tasks;
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
  if (is_kernel) {
    group->pgd = 0;
  }
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
    if (g->pgd) {
      free(g->pgd);
    }
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

  ktask_t *new_task;
  if (kernel) {
    new_task = malloc(sizeof(ktask_t));
    memset(new_task, 0, sizeof(ktask_t));
  } else {
    new_task = malloc(sizeof(utask_t));
    memset(new_task, 0, sizeof(utask_t));
  }
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
  do {
    new_task->id = ++id_seq;
    if (new_task->id == begins)
      return 0; // running out of pid
  } while (task_find(new_task->id));

  new_task->parent = current;
  new_task->name = name;

  //加入group
  task_group_add(group, new_task);

  return new_task;
}

void kernel_task_entry();

// FIXME 好多东西没free呢
static void task_destory(ktask_t *t) {
  assert(t);
  if (t->kstack) {
    kmem_page_free((void *)t->kstack, 1);
  }
  task_group_remove(t->group, t);
#ifndef NDEBUG
  printf("task_destory: destroy task %s\n", t->name);
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
void task_clean() {
  if (!list_empty(&dead_tasks)) {
    for (list_entry_t *p = list_next(&dead_tasks); p != &dead_tasks;) {
      list_entry_t *next = list_next(p);
      task_destory((ktask_t *)p);
      p = next;
    }
    list_init(&dead_tasks);
  }
}

void task_init() {
  list_init(&tasks);
  list_init(&dead_tasks);
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

void task_schd() {
  while (true) {
    hlt();
  }
  __builtin_unreachable();
}

//当前进程
pid_t task_current() {
  if (!current) {
    return 0;
  } else {
    return current->id;
  }
}

//对kernel接口
//创建user task
// entry是开始执行的虚拟地址，arg是指向函数参数的虚拟地址
//若program为0，则program_size将被忽略。program和group之间有且只有一个为非0，这意味着只有创建进程时才能指定program
pid_t task_create_user(void *program, uint32_t program_size, const char *name,
                       task_group_t *group, uintptr_t entry, int arg_count,
                       ...) {
  if (!(((program != 0) || (group != 0)) && ((program != 0) != (group != 0)))) {
    return 0;
  }

  utask_t *new_task = task_create_impl(name, false, group);
  if (!new_task) {
    return 0;
  }
  //内核栈
  new_task->base.kstack = (uintptr_t)kmem_page_alloc(1);
  if (!new_task->base.kstack) {
    task_destory(new_task);
    return 0;
  }
  //用户栈
  new_task->ustack = (uintptr_t)kmem_page_alloc(1);
  if (!new_task->ustack) {
    task_destory(new_task);
    return 0;
  }
  //设置这个新group的虚拟内存
  void *page_directory = kmem_page_alloc(1);
  if (!page_directory) {
    task_destory(new_task);
    return 0;
  }
  //存放程序映像的虚拟内存
  new_task->program = aligned_alloc(0, _4M);
  if (!new_task->program) {
    task_destory(new_task);
    return 0;
  }
  //读elf
  struct elfhdr *elf_header = program;
  if (elf_header->e_magic != ELF_MAGIC) {
    task_destory(new_task);
    return 0;
  }
  struct proghdr *program_header, *ph_end;
  // load each program segment (ignores ph flags)
  program_header =
      (struct proghdr *)((uintptr_t)elf_header + elf_header->e_phoff);
  ph_end = program_header + elf_header->e_phnum;
  for (; program_header < ph_end; program_header++) {
    // readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    memcpy(new_task->program + (program_header->p_va - 0x8000000),
           program + program_header->p_offset, program_header->p_filesz);
  }

  extern uint32_t boot_pd[];
  //以内核页表为蓝本
  memcpy(page_directory, boot_pd, _4K);
  //清空12MB开始到2GB+12MB之间的映射
  memset((void *)(uintptr_t)(((char *)page_directory) + (12 / 4 * 4)), 0,
         512 * 4);
  //把new_task->program映射到128MB的地方
  map_page_4M(page_directory, 0x8000000, new_task->program, 1,
              PTE_P | PTE_W | PTE_PS | PTE_U);
  // map用户栈（的结尾）到3GB-128M的地方
  // TODO 因为每个线程都有自己的栈，所以之后这里要用umalloc去做，这需要实现vma
  map_page_4M(page_directory, 0xB8000000, new_task->ustack, 1,
              PTE_P | PTE_W | PTE_PS | PTE_U);
  group->pgd = page_directory;

  //设置上下文和栈
  memset(&new_task->base.regs, 0, sizeof(struct registers));
  // FIXME
  assert(entry == DETECT_ENTRY);
  new_task->base.regs.eip = elf_header->e_entry;
  // FIXME
  new_task->base.regs.ebp = 0xB8000000;
  new_task->base.regs.esp = 0xB8000000 - 2 * sizeof(void *);
  //*(void **)(new_task->base.regs.esp + 4) = 999;
  //*(void **)(new_task->base.regs.esp) = 888;

  list_add(&tasks, &new_task->base.global_head);
  return new_task->base.id;
}

//创建内核线程
pid_t task_create_kernel(void (*func)(void *), void *arg, const char *name) {
  ktask_t *schd = task_find(1);
  ktask_t *new_task = task_create_impl(name, true, schd->group);
  if (!new_task)
    return 0;

  new_task->kstack = (uintptr_t)kmem_page_alloc(1);
  if (!new_task->kstack) {
    task_destory(new_task);
    return 0;
  }

  //设置上下文和内核栈
  memset(&new_task->regs, 0, sizeof(struct registers));
  new_task->regs.eip = (uint32_t)(uintptr_t)kernel_task_entry;
  new_task->regs.ebp = new_task->kstack + _4K;
  new_task->regs.esp = new_task->kstack + _4K - 2 * sizeof(void *);
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
  //把current设置为EXITED并且加入到dead_tasks链表中，之后schd线程会对它调用destory的
  current->state = EXITED;
  list_del(&current->global_head);
  list_init(&current->global_head);
  list_add(&dead_tasks, &current->global_head);
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
  // load_esp0(t->kstack + STACK_SIZE);
  t->state = RUNNING;
  ktask_t *prev = current;
  current = t;
  switch_to(&prev->regs, &t->regs);
}
