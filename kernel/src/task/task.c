#include <assert.h>
#include <avlmini.h>
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
#include <sync.h>
#include <task.h>
#include <virtual_memory.h>
#include <x86.h>

uint32_t task_inited;

// 所有的task
static struct avl_tree tasks;
// 当前运行的task
static ktask_t *current;
// id序列
static pid_t id_seq;

void task_args_init(struct task_args *dst) {
  dst->cnt = 0;
  list_init(&dst->args);
}

void task_args_add(struct task_args *dst, const char *str) {
  struct task_arg *holder = (struct task_arg *)malloc(sizeof(struct task_arg));
  assert(holder);
  holder->strlen = strlen(str);
  holder->data = (char *)malloc(holder->strlen + 1);
  assert(holder->data);
  memcpy(holder->data, str, holder->strlen);
  holder->data[holder->strlen] = '\0';
  list_add_before(&dst->args, &holder->head);
  dst->cnt++;
}

void task_args_pack(struct task_args *dst) {
  assert(dst->packed == 0);
  dst->packed = malloc(sizeof(char *) * dst->cnt);
  uint32_t i = 0;
  for (list_entry_t *p = list_next(&dst->args); p != &dst->args;
       p = list_next(p)) {
    struct task_arg *arg = (struct task_arg *)p;
    dst->packed[i++] = arg->data;
  }
}

static void task_args_destroy(struct task_args *dst) {
  uint32_t verify = 0;
  for (list_entry_t *p = list_next(&dst->args); p != &dst->args;) {
    verify++;
    struct task_arg *arg = (struct task_arg *)p;
    free(arg->data);
    void *current = p;
    p = list_next(p);
    free(current);
  }
  list_init(&dst->args);
  free((void *)dst->packed);
  assert(verify == dst->cnt);
  dst->cnt = 0;
}

static void add_task(ktask_t *new_task) {
  make_sure_int_disabled();
  void *prev = avl_tree_add(&tasks, new_task);
  assert(prev == 0);
}

static int compare_task(const void *a, const void *b) {
  const ktask_t *ta = a;
  const ktask_t *tb = b;
  return ta->id - tb->id;
}

//创建组
static task_group_t *task_group_create(bool is_kernel) {
  task_group_t *group = malloc(sizeof(task_group_t));
  if (!group) {
    return 0;
  }
  list_init(&group->tasks);
  group->task_cnt = 0;
  group->is_kernel = is_kernel;
  return group;
}

//向组中添加
static void task_group_add(task_group_t *g, ktask_t *t) {
  assert(g);
  list_add(&g->tasks, &t->group_head);
  g->task_cnt++;
  t->group = g;
}

//析构组
static void task_group_destroy(task_group_t *g) {
  if (g->vm) {
    virtual_memory_destroy(g->vm);
  }
  free(g);
}

//从组中移除
static void task_group_remove(ktask_t *t) {
  struct task_group *g = t->group;
  assert(g);
  if (g->task_cnt == 1) {
    task_group_destroy(g);
  } else {
    g->task_cnt--;
    list_del(&t->group_head);
  }
}

// 从组链表node获得对象
inline static ktask_t *task_group_head_retrieve(list_entry_t *head) {
  return (ktask_t *)(((void *)head) - sizeof(struct avl_node));
}

// 生成ID
static pid_t gen_pid() {
  make_sure_int_disabled();
  pid_t result;
  const pid_t begins = id_seq;
  do {
    result = ++id_seq;
    if (result == begins) {
      panic("running out of pid");
    }
    if (result == 0) {
      continue;
    }
  } while (task_find(result));
  return result;
}

//析构task
static void task_destory(ktask_t *t) {
  assert(t);
  if (t->args) {
    task_args_destroy(t->args);
  }
  kmem_page_free((void *)t->kstack, TASK_STACK_SIZE);
  task_group_remove(t);
  if (!t->group->is_kernel) {
    utask_t *ut = (utask_t *)t;
    kmem_page_free((void *)ut->pustack, TASK_STACK_SIZE);
    free(ut->program);
  }
#ifndef NDEBUG
  printf("task_destory: destroy task %s\n", t->name);
#endif
  free(t);
}

static ktask_t *task_create_impl(const char *name, bool kernel,
                                 task_group_t *group, struct task_args *args) {
  make_sure_int_disabled();
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
      assert(task_find(1) == 0);
    }
  }

  bool group_created = false;
  if (!group) {
    //创建一个group
    group = task_group_create(kernel);
    if (!group) {
      return 0;
    }
    group_created = true;
  }

  ktask_t *new_task;
  if (kernel) {
    new_task = malloc(sizeof(ktask_t));
    if (new_task) {
      memset(new_task, 0, sizeof(ktask_t));
    }
  } else {
    new_task = malloc(sizeof(utask_t));
    if (new_task) {
      memset(new_task, 0, sizeof(utask_t));
    }
  }
  if (!new_task) {
    if (group_created) {
      task_group_destroy(group);
    }
    return 0;
  }

  //内核栈
  new_task->kstack = (uintptr_t)kmem_page_alloc(TASK_STACK_SIZE);
  if (!new_task->kstack) {
    task_destory(new_task);
    return 0;
  }

  list_init(&new_task->group_head);
  new_task->state = CREATED;
  new_task->parent = current;
  new_task->name = name;
  //生成id
  new_task->id = gen_pid();
  //加入group
  task_group_add(group, new_task);
  if (args) {
    task_args_pack(args);
    new_task->args = args;
  } else {
    new_task->args = 0;
  }
  return new_task;
}

void kernel_task_entry();
void user_task_entry();

// 切换寄存器
// save指示是否要保存
void switch_to(bool save, void *from, void *to);

//搜索task
ktask_t *task_find(pid_t pid) {
  SMART_CRITICAL_REGION
  ktask_t key;
  key.id = pid;
  void *ret = avl_tree_find(&tasks, &key);
  return ret;
}

//显示系统中所有task
void task_display() {
  SMART_CRITICAL_REGION
  printf("\n\nTask Display   Current: %s", current->name);
  printf("\n*******************************************************************"
         "******\n");
  ktask_t key;
  key.id = 0;
  ktask_t *p = avl_tree_nearest(&tasks, &key);
  do {
    if (p) {
      printf("State:%s  ID:%d  Supervisor:%s  Group:0x%08llx  Name:%s\n",
             task_state_str(p->state), (int)p->id,
             p->group->is_kernel ? "T" : "F",
             (int64_t)(uint64_t)(uintptr_t)p->group, p->name);
      p = avl_tree_next(&tasks, p);
    }
  } while (p != 0);
  printf("*********************************************************************"
         "****\n\n");
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
    return "EXITED ";
  default:
    panic("zhu ni zhong qiu jie kuai le!");
  }
  __builtin_unreachable();
}

//初始化任务系统
void task_init() {
  avl_tree_init(&tasks, compare_task, sizeof(ktask_t), 0);
  //将当前的上下文设置为第一个任务
  ktask_t *init = task_create_impl("idle", true, 0, 0);
  if (!init) {
    panic("creating task idle failed");
  }
  init->state = RUNNING;
  init->kstack = (uintptr_t)kmem_page_alloc(TASK_STACK_SIZE);
  assert(init->kstack);
  add_task(init);
  current = init;
  load_esp0(0);
  task_inited = TASK_INITED_MAGIC;
}

void task_clean() {
  SMART_CRITICAL_REGION
  ktask_t key;
  key.id = 0;
  ktask_t *p = avl_tree_nearest(&tasks, &key);
  if (p) {
    do {
      if (p->state == EXITED) {
        ktask_t *n = avl_tree_next(&tasks, p);
        avl_tree_remove(&tasks, p);
        task_destory(p);
        p = n;
        continue;
      }
      p = avl_tree_next(&tasks, p);
    } while (p != 0);
  }
}

// idle专用的调度函数，如果有其他task需要执行，那么执行。否则hlt
// FIXME 这个是有问题的，因为task运行中可能会修改avl树
void task_schd() {
  while (true) {
    // printf("task_schd: reschdule\n");
    disable_interrupt();
    ktask_t key;
    key.id = 0;
    ktask_t *p = avl_tree_nearest(&tasks, &key);
    if (p) {
      do {
        if (p != current) {
          // printf("task_schd: switch to %s\n", p->name);
          task_switch(p);
          disable_interrupt();
          if (p->state == EXITED) {
            // printf("task_schd: task %s quited\n", p->name);
            ktask_t *n = avl_tree_next(&tasks, p);
            avl_tree_remove(&tasks, p);
            task_destory(p);
            p = n;
            continue;
          }
          // printf("task_schd: task %s yielded\n", p->name);
        }
        p = avl_tree_next(&tasks, p);
      } while (p != 0);
    }
    // printf("task_schd: hlt\n");
    enable_interrupt();
    hlt();
  }
  __builtin_unreachable();
}

// 当前进程
pid_t task_current() {
  SMART_CRITICAL_REGION
  if (!current) {
    return 0;
  } else {
    return current->id;
  }
}

// 创建user task
// entry是开始执行的虚拟地址，arg是指向函数参数的虚拟地址
// 若program为0，则program_size将被忽略。program和group之间有且只有一个为非0，这意味着只有创建进程时才能指定program
pid_t task_create_user(void *program, uint32_t program_size, const char *name,
                       task_group_t *group, uintptr_t entry,
                       struct task_args *args) {
  // FIXME 根据program_size去看底下elf处理时候有没有越界
  UNUSED(program_size);
  if (!(((program != 0) || (group != 0)) && ((program != 0) != (group != 0)))) {
    return 0;
  }
  // mdzz
  bool is_first = program ? true : false;

  //只有is_first时才能检测程序入口
  assert((entry == DETECT_ENTRY) == is_first);

  SMART_CRITICAL_REGION
  utask_t *new_task = (utask_t *)task_create_impl(name, false, group, args);
  if (!new_task) {
    return 0;
  }
  group = new_task->base.group;
  //用户栈
  new_task->pustack = (uintptr_t)kmem_page_alloc(TASK_STACK_SIZE);
  if (!new_task->pustack) {
    task_destory((struct ktask *)new_task);
    return 0;
  }
  //如果这是进程中首个线程，需要设置这个进程的虚拟内存
  if (is_first) {
    //设置这个新group的虚拟内存
    group->vm = virtual_memory_create();
    if (!group->vm) {
      task_destory((struct ktask *)new_task);
      return 0;
    }
    //存放程序映像的虚拟内存
    new_task->program = aligned_alloc(1, ROUNDUP(program_size, _4K));
    if (!new_task->program) {
      task_destory((struct ktask *)new_task);
      return 0;
    }
    memset(new_task->program, 0, ROUNDUP(program_size, _4K));
    //读elf
    struct elfhdr *elf_header = program;
    if (elf_header->e_magic != ELF_MAGIC) {
      task_destory((struct ktask *)new_task);
      return 0;
    }
    struct proghdr *program_header, *ph_end;
    // load each program segment (ignores ph flags)
    program_header =
        (struct proghdr *)((uintptr_t)elf_header + elf_header->e_phoff);
    ph_end = program_header + elf_header->e_phnum;
    for (; program_header < ph_end; program_header++) {
      // FIXME 检查越界
      memcpy(new_task->program + (program_header->p_va - USER_CODE_BEGIN),
             program + program_header->p_offset, program_header->p_filesz);
    }

    extern uint32_t kernel_pd[];
    // 复制内核页表
    virtual_memory_clone(new_task->base.group->vm, kernel_pd, KERNEL);
    // 把new_task->program映射到128MB的地方
    struct virtual_memory_area *vma = virtual_memory_alloc(
        new_task->base.group->vm, USER_CODE_BEGIN, ROUNDUP(program_size, _4K),
        PTE_P | PTE_U, CODE, false);
    assert(vma);
    bool ret = virtual_memory_map(new_task->base.group->vm, vma,
                                  USER_CODE_BEGIN, ROUNDUP(program_size, _4K),
                                  V2P((uintptr_t)new_task->program));
    assert(ret);
    if (entry == DETECT_ENTRY) {
      entry = (uintptr_t)elf_header->e_entry;
    }
  }
  //在虚拟内存中的3G-512MB是用户栈
  new_task->vustack = USER_STACK_BEGIN;
  struct virtual_memory_area *vma = virtual_memory_alloc(
      new_task->base.group->vm, new_task->vustack, _4K * TASK_STACK_SIZE,
      PTE_P | PTE_W | PTE_U, STACK, false);
  assert(vma);
  int ret = virtual_memory_map(new_task->base.group->vm, vma, new_task->vustack,
                               _4K * TASK_STACK_SIZE, V2P(new_task->pustack));
  assert(ret);

  //设置上下文和内核栈
  memset(&new_task->base.regs, 0, sizeof(struct registers));
  new_task->base.regs.eip = (uint32_t)(uintptr_t)user_task_entry;
  new_task->base.regs.ebp = 0;
  uintptr_t kstack_top = new_task->base.kstack + _4K * TASK_STACK_SIZE;
  new_task->base.regs.esp = kstack_top - sizeof(void *) * 4;
  //用户代码入口
  *(uintptr_t *)(new_task->base.regs.esp + 12) = entry;
  //用户栈
  *(uintptr_t *)(new_task->base.regs.esp + 8) =
      new_task->vustack + _4K * TASK_STACK_SIZE;
  // TODO 等到实现了用户的业内内存分配，这里就要换成真的参数了
  // argc
  *(uintptr_t *)(new_task->base.regs.esp + 4) = (uintptr_t)9710;
  // argv
  *(void **)(new_task->base.regs.esp) = (void *)9999;

  add_task((ktask_t *)new_task);
  return new_task->base.id;
}

//创建内核线程
pid_t task_create_kernel(void (*func)(int, char **), const char *name,
                         struct task_args *args) {
  SMART_CRITICAL_REGION
  ktask_t *schd = task_find(1);
  ktask_t *new_task = task_create_impl(name, true, schd->group, args);
  if (!new_task)
    return 0;

  //设置上下文和内核栈
  memset(&new_task->regs, 0, sizeof(struct registers));
  new_task->regs.eip = (uint32_t)(uintptr_t)kernel_task_entry;
  new_task->regs.ebp = new_task->kstack + _4K * TASK_STACK_SIZE;
  new_task->regs.esp = new_task->regs.ebp - 3 * sizeof(void *);
  *(void **)(new_task->regs.esp + 8) = (void *)args->packed; // argv
  *(void **)(new_task->regs.esp + 4) = (void *)args->cnt;    // argc
  *(void **)(new_task->regs.esp) = (void *)func;

  add_task(new_task);
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
void task_exit() {
  disable_interrupt();
  //找到idle task，它的pid是1
  ktask_t *schd = task_find(1);
  //把current设置为EXITED
  current->state = EXITED;
  //切换到schd
  task_switch(schd);
}

//切换到另一个task
void task_switch(ktask_t *next) {
  make_sure_int_disabled();
  assert(current);
  assert(next);
  if (next == current) {
    panic("task_switch");
  }
  extern uint32_t kernel_pd[];
  if (current->state != EXITED) {
    current->state = YIELDED;
  }
  if (!current->group->is_kernel || !next->group->is_kernel) {
    //不是内核到内核的切换
    // 1. 切换页表
    union {
      struct CR3 cr3;
      uintptr_t val;
    } cr3;
    cr3.val = 0;
    //如果是用户到内核，那么切换到kernel_pd
    if (next->group->is_kernel) {
      set_cr3(&cr3.cr3, V2P((uintptr_t)kernel_pd), false, false);
    } else {
      //如果是用户到用户或者内核到用户，那么切换到PCB里的页表
      set_cr3(&cr3.cr3, V2P((uintptr_t)next->group->vm->page_directory), false,
              false);
    }
    lcr3(cr3.val);

    // 2.切换tss栈
    if (!next->group->is_kernel) {
      load_esp0(next->kstack + _4K * TASK_STACK_SIZE);
    } else {
      load_esp0(0);
    }
  }
  next->state = RUNNING;
  ktask_t *prev = current;
  current = next;
  // 切换寄存器，包括eip、esp和ebp
  switch_to(prev->state != EXITED, &prev->regs, &next->regs);
}
