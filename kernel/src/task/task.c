#include <assert.h>
#include <atomic.h>
#include <avlmini.h>
#include <clock.h>
#include <compiler_helper.h>
#include <elf.h>
#include <gdt.h>
#include <list.h>
#include <memlayout.h>
#include <memory_barrier.h>
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

// #define VERBOSE

uint32_t task_inited;

// 所有的task
static struct avl_tree tasks;
// 当前运行的task
static ktask_t *current;
// id序列
static pid_t id_seq;
// 调度队列，在这个队列中的task都是可执行的，也就是它没有等待锁或者io或者sleep
static list_entry_t ready_queue;

static __always_inline uint32_t task_count() {
  make_sure_schd_disabled();
  return tasks.count;
}

// 从组链表node获得对象
inline static ktask_t *task_group_head_retrieve(list_entry_t *head) {
  return (ktask_t *)(((void *)head) - sizeof(struct avl_node) * 2);
}

// 从ready链表node获得对象
inline static ktask_t *task_ready_queue_head_retrieve(list_entry_t *head) {
  return (ktask_t *)(((void *)head) - sizeof(struct avl_node));
}

static ktask_t *ready_queue_get() {
  list_entry_t *entry = list_next(&ready_queue);
  if (entry == &ready_queue) {
    return 0;
  }
  return task_ready_queue_head_retrieve(entry);
}

static int compare_task_by_ts(const void *a, const void *b) {
  const ktask_t *ta = a;
  const ktask_t *tb = b;
  if (ta->tslice > tb->tslice)
    return 1;
  if (ta->tslice < tb->tslice)
    return -1;
  if (ta->tslice == tb->tslice)
    return 0;
  __builtin_unreachable();
}

static void ready_queue_put(ktask_t *t) {
  list_init(&t->ready_queue_head);
  list_sort_add(&ready_queue, &t->ready_queue_head, compare_task_by_ts,
                sizeof(struct avl_node));
}

// 每个调度周期调一次
void task_handle_wait() {
  // 遍历全部任务，如果有WAITING状态的任务等到了他要的东西，就把它放到readyqueue
  for (ktask_t *t = avl_tree_first(&tasks); t != 0;
       t = avl_tree_next(&tasks, t)) {
    if (t->state == WAITING) {
      assert(t->ready_queue_head.next == 0 && t->ready_queue_head.prev == 0);
      switch (t->wait_type) {
      case SLEEP:
        if (clock_get_ticks() * TICK_TIME_MS >= t->wait_ctx.sleep.after) {
          goto PUTBACK;
        }
        continue;
      case MUTEX:
        if (false) {
          goto PUTBACK;
        }
        continue;
      default:
        panic("unhandled wait");
      }
    PUTBACK:
      t->state = YIELDED;
      ready_queue_put(t);
    }
  }
}

struct virtual_memory *current_vm;
struct virtual_memory *virtual_memory_current() {
  return current_vm;
}

void task_args_init(struct task_args *dst) {
  memset(dst, 0, sizeof(struct task_args));
  list_init(&dst->args);
}

void task_args_add(struct task_args *dst, const char *str,
                   struct virtual_memory *vm, bool use_umalloc) {
  struct task_arg *holder = (struct task_arg *)malloc(sizeof(struct task_arg));
  assert(holder);
  memset(holder, 0, sizeof(struct task_arg));
  holder->strlen = strlen(str);
  if (!use_umalloc) {
    holder->data = (uintptr_t)malloc(holder->strlen + 1);
    holder->vdata = (uintptr_t)holder->data;
  } else {
    uintptr_t virtual, physical;
    if (!(virtual = umalloc(vm, holder->strlen + 1, false, 0, &physical))) {
      abort();
    }
    holder->data = physical;
    holder->vdata = virtual;
  }
  assert(holder->data && holder->vdata);
  // 这data是在free region所以需要临时map到内核来才能访问
  char *data_access;
  if (use_umalloc) {
    data_access = free_region_access(holder->data, holder->strlen + 1);
  } else {
    data_access = (char *)holder->data;
  }
  memcpy(data_access, str, holder->strlen);
  data_access[holder->strlen] = '\0';
  if (use_umalloc)
    free_region_no_access(data_access);
  list_add_before(&dst->args, &holder->head);
  dst->cnt++;
}

static void task_args_pack(struct task_args *dst, struct virtual_memory *vm,
                           bool use_umalloc) {
  assert(dst->packed == 0);
  if (!use_umalloc) {
    dst->packed = (uintptr_t)malloc(sizeof(char *) * dst->cnt);
  } else {
    uintptr_t virtual, physical;
    if (!(virtual =
              umalloc(vm, sizeof(char *) * dst->cnt, false, 0, &physical))) {
      abort();
    }
    dst->packed = physical;
    dst->vpacked = virtual;
  }
  const char **data_access;
  if (use_umalloc) {
    data_access = free_region_access(dst->packed, sizeof(char *) * dst->cnt);
  } else {
    data_access = (const char **)dst->packed;
  }
  uint32_t i = 0;
  for (list_entry_t *p = list_next(&dst->args); p != &dst->args;
       p = list_next(p)) {
    struct task_arg *arg = (struct task_arg *)p;
    data_access[i++] = (const char *)arg->vdata;
  }
  if (use_umalloc) {
    free_region_no_access(data_access);
  }
}

// 对于umalloc出来的内存，这里不作处理，等进程销毁时自动处理
void task_args_destroy(struct task_args *dst, bool free_data) {
  uint32_t verify = 0;
  for (list_entry_t *p = list_next(&dst->args); p != &dst->args;) {
    verify++;
    struct task_arg *arg = (struct task_arg *)p;
    if (free_data) {
      free((void *)arg->data);
    }
    void *current = p;
    p = list_next(p);
    free(current);
  }
  list_init(&dst->args);
  if (dst->packed) {
    if (free_data) {
      free((void *)dst->packed);
    }
  }
  assert(verify == dst->cnt);
  dst->cnt = 0;
}

static struct avl_tree ret_val_tree;
struct ret_val {
  struct avl_node avl_head;
  pid_t pid;
  int32_t val;
};
static int compare_ret_val(const void *a, const void *b) {
  const struct ret_val *ta = (const struct ret_val *)a;
  const struct ret_val *tb = (const struct ret_val *)b;
  if (ta->pid > tb->pid)
    return 1;
  else if (ta->pid < tb->pid)
    return -1;
  else
    return 0;
}
static void record_ret_val(pid_t id, int32_t val) {
  struct ret_val *record = malloc(sizeof(struct ret_val));
  assert(record);
  memset(record, 0, sizeof(struct ret_val));
  avl_node_init(&record->avl_head);
  record->pid = id;
  record->val = val;
  void *prev = avl_tree_add(&ret_val_tree, record);
  if (prev) {
    panic("record_ret_val avl_tree_add");
  }
}
static void del_ret_val(pid_t id) {
  struct ret_val find;
  find.pid = id;
  struct ret_val *record = avl_tree_find(&ret_val_tree, &find);
  if (record) {
    avl_tree_remove(&ret_val_tree, record);
    free(record);
  }
}
static int32_t get_ret_val(pid_t id) {
  struct ret_val find;
  find.pid = id;
  struct ret_val *record = avl_tree_find(&ret_val_tree, &find);
  if (!record) {
    abort();
  }
  return record->val;
}

static void add_task(ktask_t *new_task) {
  avl_node_init(&new_task->global_head);
  make_sure_schd_disabled();
  void *prev = avl_tree_add(&tasks, &new_task->global_head);
  if (prev) {
    abort();
  }
  ready_queue_put(new_task);
}

static int compare_task(const void *a, const void *b) {
  const ktask_t *ta = a;
  const ktask_t *tb = b;
  if (ta->id > tb->id)
    return 1;
  if (ta->id < tb->id)
    return -1;
  if (ta->id == tb->id)
    return 0;
  __builtin_unreachable();
}

//创建组
static task_group_t *task_group_create(bool is_kernel) {
  task_group_t *group = malloc(sizeof(task_group_t));
  if (!group) {
    return 0;
  }
  memset(group, 0, sizeof(sizeof(task_group_t)));
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

// 生成ID
static pid_t gen_pid() {
  make_sure_schd_disabled();
  pid_t result;
  const pid_t begins = atomic_load(&id_seq);
  do {
    result = atomic_add(&id_seq, 1);
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
#ifdef VERBOSE
  printf("task_destory: destroy task %s(%lld)\n", t->name, (int64_t)t->id);
#endif
  if (t->args) {
    task_args_destroy(t->args, t->group->is_kernel);
    free(t->args);
  }
  kmem_page_free((void *)t->kstack, TASK_STACK_SIZE);
  task_group_remove(t);
  if (!t->group->is_kernel) {
    utask_t *ut = (utask_t *)t;
    kmem_page_free((void *)ut->pustack, TASK_STACK_SIZE);
    free(ut->program);
  }
  free(t->name);
  free(t);
}

static ktask_t *task_create_impl(const char *name, bool kernel,
                                 task_group_t *group) {
  make_sure_schd_disabled();
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
    assert(new_task);
    memset(new_task, 0, sizeof(ktask_t));
  } else {
    new_task = malloc(sizeof(utask_t));
    assert(new_task);
    memset(new_task, 0, sizeof(utask_t));
  }
  if (!new_task) {
    if (group_created) {
      task_group_destroy(group);
    }
    return 0;
  }
  if (task_current())
    new_task->tslice = task_current()->tslice;

  // 内核栈
  new_task->kstack = (uintptr_t)kmem_page_alloc(TASK_STACK_SIZE);
  if (!new_task->kstack) {
    task_destory(new_task);
    return 0;
  }

  list_init(&new_task->group_head);
  new_task->state = CREATED;
  new_task->parent = current;
  new_task->name = malloc(strlen(name) + 1);
  strcpy(new_task->name, name);
  // 生成id
  new_task->id = gen_pid();
  // 移除此pid的返回值记录，因为此前可能有一个进程用过这个pid
  del_ret_val(new_task->id);
  // 加入group
  task_group_add(group, new_task);
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
  for (ktask_t *p = avl_tree_first(&tasks); p != 0;
       p = avl_tree_next(&tasks, p)) {
    printf("State:%s ID:%d K:%s Group:0x%08llx TS:%lld Name:%s\n",
           task_state_str(p->state), (int)p->id,
           p->group->is_kernel ? "T" : "F",
           (int64_t)(uint64_t)(uintptr_t)p->group, p->tslice, p->name);
  }

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
  case WAITING:
    return "WAITING";
  default:
    panic("zhu ni zhong qiu jie kuai le!");
  }
  __builtin_unreachable();
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

// 要维护的性质是
// ready队列里面只能是created或yielded任务
// 也就是说，一个任务运行时，一定不在ready队列里
// force指示即使调度队列中的任务slice比当前任务更高，但还是切换到该任务
// allow_idle指示是否允许切到idle
// tostate指示切走后，当前任务的状态
bool task_schd(bool force, bool allow_idle, enum task_state tostate) {
  SMART_NOINT_REGION
  // 如果调度队列有task
  ktask_t *t = ready_queue_get();
  if (t) {
    if (allow_idle || t->id != 1) {
      if (force || t->tslice < task_current()->tslice) {
        // 如果t不如当前任务的时间片，就执行它
        task_switch(t, true, tostate);
        return true;
      } else {
        // terminal_fgcolor(CGA_COLOR_LIGHT_BLUE);
        // printf("%lld not switching to %lld\n", (int64_t)task_current()->id,
        //        (int64_t)t->id);
        // terminal_default_color();
      }
    }
  }
  return false;
}

void task_idle() {
  task_preemptive_set(false);
  while (true) {
    // 把idle的时间片设置到很大，这样它永远是最低优先级的
    task_current()->tslice = 0x8000000000000000;
    while (task_schd(false, false, YIELDED)) {
      task_preemptive_set(false);
#ifdef VERBOSE
      printf("idle: back\n");
#endif
      for (ktask_t *t = avl_tree_first(&tasks); t != 0;) {
        if (t->state == EXITED) {
          ktask_t *n = avl_tree_next(&tasks, t);
          avl_tree_remove(&tasks, t);
          task_destory(t);
          t = n;
          continue;
        }
        t = avl_tree_next(&tasks, t);
      }
#ifdef VERBOSE
      task_display();
#endif
    }
    hlt();
  }
}

// 这个volatile是为了task_disable_preemptive里的copy不被优化掉
static volatile bool task_preemptive = false;
bool task_preemptive_enabled() {
  bool copy = task_preemptive;
  memory_barrier(ACQUIRE);
  return copy;
}
void task_preemptive_set(bool val) {
  memory_barrier(RELEASE);
  task_preemptive = val;
}

// 当前进程
ktask_t *task_current() {
  SMART_CRITICAL_REGION
  if (!current) {
    return 0;
  } else {
    return current;
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

  // 只有is_first时才能检测程序入口
  assert((entry == DEFAULT_ENTRY) == is_first);

  SMART_CRITICAL_REGION
  utask_t *new_task = (utask_t *)task_create_impl(name, false, group);
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
    virtual_memory_clone(new_task->base.group->vm, kernel_pd, UKERNEL);
    // 把new_task->program映射到128MB的地方
    struct virtual_memory_area *vma = virtual_memory_alloc(
        new_task->base.group->vm, USER_CODE_BEGIN, ROUNDUP(program_size, _4K),
        PTE_P | PTE_U, UCODE, false);
    assert(vma);
    virtual_memory_map(new_task->base.group->vm, vma, USER_CODE_BEGIN,
                       ROUNDUP(program_size, _4K),
                       V2P((uintptr_t)new_task->program));
    if (entry == DEFAULT_ENTRY) {
      entry = (uintptr_t)elf_header->e_entry;
    }
  }
  //在虚拟内存中的用户栈
  new_task->vustack = USER_STACK_BEGIN;
  struct virtual_memory_area *vma = virtual_memory_alloc(
      new_task->base.group->vm, new_task->vustack, _4K * TASK_STACK_SIZE,
      PTE_P | PTE_W | PTE_U, USTACK, false);
  assert(vma);
  virtual_memory_map(new_task->base.group->vm, vma, new_task->vustack,
                     _4K * TASK_STACK_SIZE, V2P(new_task->pustack));

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
  if (args) {
    // 拷贝参数
    new_task->base.args = malloc(sizeof(struct task_args));
    assert(new_task->base.args);
    task_args_init(new_task->base.args);
    for (list_entry_t *p = list_next(&args->args); p != &args->args;
         p = list_next(p)) {
      struct task_arg *arg = (struct task_arg *)p;
      task_args_add(new_task->base.args, (const char *)arg->data,
                    new_task->base.group->vm, true);
    }
    task_args_pack(new_task->base.args, new_task->base.group->vm, true);
    *(uint32_t *)(new_task->base.regs.esp + 4) =
        new_task->base.args->cnt; // argc
    *(uintptr_t *)(new_task->base.regs.esp) =
        new_task->base.args->vpacked; // argv
                                      // #ifdef VERBOSE
    //     printf("pid %lld pd is 0x%08llx\n", (int64_t)new_task->base.id,
    //            (int64_t)V2P((uintptr_t)new_task->base.group->vm->page_directory));
    //     printf("checking args for pid %lld\n", (int64_t)new_task->base.id);
    //     void *access = free_region_access(new_task->base.args->packed,
    //                                       new_task->base.args->cnt);
    //     assert(access);
    //     for (uint32_t i = 0; i < new_task->base.args->cnt; i++) {
    //       uintptr_t pa =
    //       linear2physical(new_task->base.group->vm->page_directory,
    //                                      *(uint32_t *)(access + i * 4));
    //       char *arg_access = free_region_access(pa, 4096);
    //       printf("%d ap:0x%08llx va:0x%08llx pa:0x%08llx str:%s\n", i,
    //              (int64_t)(uintptr_t)arg_access,
    //              (int64_t) * (uint32_t *)(access + i * 4), (int64_t)pa,
    //              arg_access);
    //       free_region_no_access(arg_access);
    //     }
    //     free_region_no_access(access);
    //     printf("\n");
    // #endif
  } else {
    new_task->base.args = 0;
    *(uintptr_t *)(new_task->base.regs.esp + 4) = (uintptr_t)0; // argc
    *(void **)(new_task->base.regs.esp) = (void *)0;            // argv
  }

  add_task((ktask_t *)new_task);
  return new_task->base.id;
}

//创建内核线程
pid_t task_create_kernel(int (*func)(int, char **), const char *name,
                         struct task_args *args) {
  SMART_CRITICAL_REGION
  ktask_t *schd = task_find(1);
  ktask_t *new_task = task_create_impl(name, true, schd->group);
  if (!new_task)
    return 0;

  // 在task_destroy时候销毁args
  if (args) {
    task_args_pack(args, 0, false);
    new_task->args = args;
  } else {
    new_task->args = 0;
  }

  //设置上下文和内核栈
  memset(&new_task->regs, 0, sizeof(struct registers));
  new_task->regs.eip = (uint32_t)(uintptr_t)kernel_task_entry;
  new_task->regs.ebp = new_task->kstack + _4K * TASK_STACK_SIZE;
  new_task->regs.esp = new_task->regs.ebp - 3 * sizeof(void *);
  if (new_task->args) {
    *(void **)(new_task->regs.esp + 8) = (void *)args->packed; // argv
    *(void **)(new_task->regs.esp + 4) = (void *)args->cnt;    // argc
  } else {
    *(void **)(new_task->regs.esp + 8) = (void *)0; // argv
    *(void **)(new_task->regs.esp + 4) = (void *)0; // argc
  }
  *(void **)(new_task->regs.esp) = (void *)func;

  add_task(new_task);
  return new_task->id;
}

// 等待进程结束
int32_t task_join(pid_t pid) {
  panic("not implemented");
  return get_ret_val(pid);
}

// 放弃当前进程时间片
void task_yield() {
  SMART_NOINT_REGION
  task_current()->tslice++;
  task_schd(true, false, YIELDED);
}

// 将当前进程挂起一定毫秒数
void task_sleep(uint64_t millisecond) {
  assert(task_current()->id != 1);
  {
    SMART_NOINT_REGION
    task_current()->tslice++;
    task_current()->wait_type = SLEEP;
    task_current()->wait_ctx.sleep.after =
        clock_get_ticks() * TICK_TIME_MS + millisecond;
    if (!task_schd(true, false, WAITING)) {
      // 没有任务做了，切到idle
      task_switch(task_find(1), true, WAITING);
    }
    assert(clock_get_ticks() * TICK_TIME_MS >=
           task_current()->wait_ctx.sleep.after);
  }
}

// TODO 通知等待此线程的线程
static void task_quit(int32_t ret) {
#ifdef VERBOSE
  printf("task %lld returned %d\n", (int64_t)task_current()->id, ret);
#endif
  disable_interrupt();
  // 保存返回值供其他task查询
  record_ret_val(task_current()->id, ret);
  // 找到idle task，它的pid是1
  ktask_t *schd = task_find(1);
  // 切换到schd
  task_switch(schd, task_preemptive_enabled(), EXITED);
}

// 退出当前进程
void task_exit(int32_t ret) {
  if (ret < 0) {
    panic("task_exit ret < 0");
  }
  task_quit(ret);
}

void task_terminate(int32_t ret) {
  if (ret >= 0) {
    panic("task_terminate ret >= 0");
  }
  // TODO 把同vm的其他线程全部杀了
  // TODO 为了尽量减少资源泄漏，强杀进程都要做stack rewind，这怎么做？
  task_quit(ret);
}

//切换到另一个task
void task_switch(ktask_t *next, bool schd, enum task_state tostate) {
  SMART_NOINT_REGION
  assert(current);
  assert(next && (next->state == CREATED || next->state == YIELDED));
  if (next == current) {
    panic("task_switch");
  }
  extern uint32_t kernel_pd[];
  current->state = tostate;
  if (!current->group->is_kernel || !next->group->is_kernel) {
    //不是内核到内核的切换
    // 1. 切换页表
    union {
      struct CR3 cr3;
      uintptr_t val;
    } cr3;
    cr3.val = 0;
    // 切换到PCB里的页表
    set_cr3(&cr3.cr3, V2P((uintptr_t)next->group->vm->page_directory), false,
            false);
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
  current_vm = next->group->vm;
  // next在ready队列里面，prev不在队列里面
  assert(next->ready_queue_head.next != 0 && next->ready_queue_head.prev != 0);
  assert(prev->ready_queue_head.next == 0 && prev->ready_queue_head.prev == 0);
  // 从队列移除next
  list_del(&next->ready_queue_head);
  next->ready_queue_head.next = 0;
  next->ready_queue_head.prev = 0;
  // 向队列加入prev
  if (prev->state == YIELDED) {
    ready_queue_put(prev);
  }
  task_preemptive_set(schd);
  // terminal_fgcolor(CGA_COLOR_BLUE);
  // printf("switch from %s(%lld) to %s(%lld)\n", prev->name, (int64_t)prev->id,
  //        next->name, (int64_t)next->id);
  // terminal_default_color();
  // 切换寄存器，包括eip、esp和ebp
  switch_to(prev->state != EXITED, &prev->regs, &next->regs);
  assert((reflags() & FL_IF) == 0);
}

// 初始化任务系统
void task_init() {
  assert(TASK_TIME_SLICE_MS >= TICK_TIME_MS &&
         TASK_TIME_SLICE_MS % TICK_TIME_MS == 0);
  avl_tree_init(&tasks, compare_task, sizeof(ktask_t), 0);
  list_init(&ready_queue);
  avl_tree_init(&ret_val_tree, compare_ret_val, sizeof(struct ret_val), 0);

  // 将当前的上下文设置为第一个任务
  ktask_t *init = task_create_impl("idle", true, 0);
  if (!init) {
    panic("creating task idle failed");
  }
  init->state = RUNNING;
  init->kstack = (uintptr_t)kmem_page_alloc(TASK_STACK_SIZE);
  assert(init->kstack);
  add_task(init);
  current = init;
  load_esp0(0);
  // 设置当前vm为kernelvm
  init->group->vm = &kernel_vm;
  current_vm = &kernel_vm;
  // 从队列移除init
  list_del(&init->ready_queue_head);
  init->ready_queue_head.next = 0;
  init->ready_queue_head.prev = 0;
}
