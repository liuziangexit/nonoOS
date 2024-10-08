#include <assert.h>
#include <avlmini.h>
#include <clock.h>
#include <compiler_helper.h>
#include <elf.h>
#include <gdt.h>
#include <kernel_object.h>
#include <list.h>
#include <memlayout.h>
#include <memory_manager.h>
#include <mmu.h>
#include <panic.h>
#include <ring_buffer.h>
#include <shell.h>
#include <stdatomic.h>
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
// 调度队列，在这个队列中的task都是可执行的，也就是它没有等待锁或者io或者sleep
static list_entry_t ready_queue;

uint32_t task_count() {
  make_sure_int_disabled();
  return tasks.count;
}

static ktask_t *ready_queue_get() {
  SMART_CRITICAL_REGION
  list_entry_t *entry = list_next(&ready_queue);
  if (entry == &ready_queue) {
    return 0;
  }
  return task_ready_queue_head_retrieve(entry);
}

static int compare_task_by_ts(const void *a, const void *b) {
  const ktask_t *ta = a;
  const ktask_t *tb = b;
  // printf("A: id %lld ts %lld\n", (int64_t)ta->id, (int64_t)ta->tslice);
  // printf("B: id %lld ts %lld\n", (int64_t)tb->id, (int64_t)tb->tslice);
  if (ta->tslice > tb->tslice)
    return 1;
  if (ta->tslice < tb->tslice)
    return -1;
  if (ta->tslice == tb->tslice)
    return 0;
  __unreachable;
}

void ready_queue_put(ktask_t *t) {
  SMART_CRITICAL_REGION
  assert(t->ready_queue_head.next == 0 || t->ready_queue_head.prev == 0);
  list_init(&t->ready_queue_head);
  list_sort_add(&ready_queue, &t->ready_queue_head, compare_task_by_ts,
                sizeof(struct avl_node));
}

// 每个调度周期调一次
void task_handle_wait() {
  make_sure_int_disabled();
  // 遍历全部任务，如果有WAITING状态的任务等到了他要的东西，就把它放到readyqueue
  for (ktask_t *t = avl_tree_first(&tasks); t != 0;
       t = avl_tree_next(&tasks, t)) {
    if (t->state == WAITING) {
      assert(t->ready_queue_head.next == 0 || t->ready_queue_head.prev == 0);
      if (t->wait_type == WAIT_SLEEP) {
        if (clock_get_ticks() * TICK_TIME_MS >= t->wait_ctx.sleep.after) {
          t->state = YIELDED;
          ready_queue_put(t);
        }
      }
      // 处理timedwait没有等到mutex而超时的情况
      if (t->wait_type == WAIT_MUTEX_TIMED) {
        if (t->wait_ctx.mutex.after != 0 &&
            clock_get_ticks() * TICK_TIME_MS >= t->wait_ctx.mutex.after) {
          t->wait_ctx.mutex.timeout = true;
          // 可以放回调度队列了
          t->state = YIELDED;
          ready_queue_put(t);
          // 需要从waitors中移除
          mutex_t *mut = kernel_object_get(t->wait_ctx.mutex.mutex_id);
          for (uint32_t i = 0; i < vector_count(&mut->waitors); i++) {
            if (*(pid_t *)vector_get(&mut->waitors, i) == t->id) {
              vector_remove(&mut->waitors, i);
              break;
            }
          }
        }
      }
      // 处理timedwait没有等到cv而超时的情况
      if (t->wait_type == WAIT_CV_TIMED) {
        if (t->wait_ctx.cv.after != 0 &&
            clock_get_ticks() * TICK_TIME_MS >= t->wait_ctx.cv.after) {
          t->wait_ctx.cv.timeout = true;
          // 可以放回调度队列了
          t->state = YIELDED;
          ready_queue_put(t);
          // 需要从waitors中移除
          condition_variable_t *cv = kernel_object_get(t->wait_ctx.cv.cv_id);
          for (uint32_t i = 0; i < vector_count(&cv->waitors); i++) {
            if (*(pid_t *)vector_get(&cv->waitors, i) == t->id) {
              vector_remove(&cv->waitors, i);
              break;
            }
          }
        }
      }
    }
  }
}

struct virtual_memory *current_vm;
struct virtual_memory *virtual_memory_current() {
  return (struct virtual_memory *)(uintptr_t)atomic_load(
      (uint32_t *)&current_vm);
}

void task_args_init(struct task_args *dst) {
  memset(dst, 0, sizeof(struct task_args));
  list_init(&dst->args);
}

void task_args_add(struct task_args *dst, const char *str,
                   struct virtual_memory *vm, bool use_umalloc,
                   uint32_t vm_mut) {
  struct task_arg *holder = (struct task_arg *)malloc(sizeof(struct task_arg));
  assert(holder);
  memset(holder, 0, sizeof(struct task_arg));
  holder->strlen = strlen(str);
  if (!use_umalloc) {
    holder->data = (uintptr_t)malloc(holder->strlen + 1);
    holder->vdata = (uintptr_t)holder->data;
  } else {
    uintptr_t virtual, physical;
    if (!(virtual =
              umalloc(vm, holder->strlen + 1, false, 0, &physical, vm_mut))) {
      abort();
    }
    holder->data = physical;
    holder->vdata = virtual;
  }
  assert(holder->data && holder->vdata);
  // 这data是在free region所以需要临时map到内核来才能访问
  char *data_access;
  if (use_umalloc) {
    data_access = free_region_access(task_current()->group->vm,
                                     task_current()->group->vm_mutex,
                                     holder->data, holder->strlen + 1);
  } else {
    data_access = (char *)holder->data;
  }
  memcpy(data_access, str, holder->strlen);
  data_access[holder->strlen] = '\0';
  if (use_umalloc)
    free_region_finish_access(task_current()->group->vm,
                              task_current()->group->vm_mutex, data_access);
  list_add_before(&dst->args, &holder->head);
  dst->cnt++;
}

// 使用task_args.args中每个task_arg的字符串地址组成一个char[][]
// 这个char[][]就是应用程序可以访问的char[][] argv了
static void task_args_pack(struct task_args *dst, struct virtual_memory *vm,
                           bool use_umalloc, uint32_t mut_id) {
  assert(dst->packed == 0);

  if (dst->cnt == 0) {
    // 特殊处理
    // 还是申请32字节的位置作为argv
    // 这样到时候销毁的时候也可以正常被回收
    if (!use_umalloc) {
      dst->packed = (uintptr_t)malloc(32);
    } else {
      uintptr_t virtual, physical;
      if (!(virtual = umalloc(vm, 32, false, 0, &physical, mut_id))) {
        abort();
      }
      dst->packed = physical;
      dst->vpacked = virtual;
    }
    return;
  }

  if (!use_umalloc) {
    dst->packed = (uintptr_t)malloc(sizeof(char *) * dst->cnt);
  } else {
    uintptr_t virtual, physical;
    if (!(virtual = umalloc(vm, sizeof(char *) * dst->cnt, false, 0, &physical,
                            mut_id))) {
      abort();
    }
    dst->packed = physical;
    dst->vpacked = virtual;
  }
  const char **data_access;
  if (use_umalloc) {
    data_access = free_region_access(task_current()->group->vm,
                                     task_current()->group->vm_mutex,
                                     dst->packed, sizeof(char *) * dst->cnt);
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
    free_region_finish_access(task_current()->group->vm,
                              task_current()->group->vm_mutex, data_access);
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
    void *c = p;
    p = list_next(p);
    free(c);
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

static void add_task(ktask_t *new_task) {
  make_sure_schd_disabled();
  avl_node_init(&new_task->global_head);
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
  __unreachable;
}

static int compare_kern_obj_id(const void *a, const void *b) {
  const struct kern_obj_id *ta = a;
  const struct kern_obj_id *tb = b;
  if (ta->id > tb->id)
    return 1;
  if (ta->id < tb->id)
    return -1;
  if (ta->id == tb->id)
    return 0;
  __unreachable;
}

// 创建组
static task_group_t *task_group_create(bool is_kernel) {
  task_group_t *group = malloc(sizeof(task_group_t));
  if (!group) {
    printf_color(
        CGA_COLOR_LIGHT_YELLOW,
        "failed to create task_group_create(malloc group struct failed)\n");
    return 0;
  }
  memset(group, 0, sizeof(sizeof(task_group_t)));
  list_init(&group->tasks);

  // 初始化记录内核对象id的avl树
  avl_tree_init(&group->kernel_objects, compare_kern_obj_id,
                sizeof(struct kern_obj_id), 0);

  // 内核进程组的input_buffer_mutex是之后在kernel.c中创建的
  if (task_inited == TASK_INITED_MAGIC) {
    group->input_buffer_mutex = mutex_create();
    group->input_buffer_cv = condition_variable_create();

    // 用当前正在创建的线程组引用该mutex
    kernel_object_ref(group, group->input_buffer_mutex);
    kernel_object_ref(group, group->input_buffer_cv);

    // 当前正运行的线程组取消引用该mutex
    if (task_current()->group != group) {
      kernel_object_unref(task_current()->group, group->input_buffer_mutex,
                          true);
      kernel_object_unref(task_current()->group, group->input_buffer_cv, true);
    }
  }

  void *input_buf = malloc(TASK_INPUT_BUFFER_LEN);
  if (!input_buf) {
    printf_color(
        CGA_COLOR_LIGHT_YELLOW,
        "failed to create task_group_create(malloc input buffer failed)\n");
    free(group);
    return 0;
  }
  ring_buffer_init(&group->input_buffer, input_buf, TASK_INPUT_BUFFER_LEN);

  group->task_cnt = 0;
  group->is_kernel = is_kernel;

  // 在kernel.c中对第一个进程组（也就是内核进程组）的vm是特殊处理的，
  // 所以在这里检测，如果发现是来自kernel.c的调用，这里就不做
  // 内核进程组的vm_mutex是之后在kernel.c中创建的
  if (task_inited == TASK_INITED_MAGIC) {
    group->vm_mutex = mutex_create();
    // 用当前正在创建的线程组引用该mutex
    kernel_object_ref(group, group->vm_mutex);
    // 当前正运行的线程组取消引用该mutex
    if (group != task_current()->group) {
      kernel_object_unref(task_current()->group, group->vm_mutex, true);
    }
  }
  return group;
}

// 向组中添加
static void task_group_add(task_group_t *g, ktask_t *t) {
  assert(g);
  list_add(&g->tasks, &t->group_head);
  g->task_cnt++;
  t->group = g;
}

static void unref_kernel_objs(void *ko) {
  make_sure_schd_disabled();
  const struct kern_obj_id *id = ko;
  kernel_object_unref(task_current()->group, id->id, false);
  free(ko);
}

// 析构组
static void task_group_destroy(task_group_t *g) {
  assert(g);
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "task_group_destroy 0x%08llx\n",
               (int64_t)(uint64_t)(uintptr_t)g);
#endif
  if (g->input_buffer.buf) {
    free(g->input_buffer.buf);
  }
  if (g->vm) {
    virtual_memory_destroy(g->vm);
  }
  if (!g->is_kernel) {
    if (g->program) {
      free(g->program);
    }
  }
  // 取消引用内核对象
  avl_tree_clear(&g->kernel_objects, unref_kernel_objs);
  free(g);
}

// 从组中移除
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

// 析构task
void task_destroy(ktask_t *t) {
  assert(t);
  // 这个检查已经在kernel_object框架里做了
  // assert(t->ref_count == 0);
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "task_destroy: destroy task %s(%lld)\n",
               t->name, (int64_t)t->id);
#endif
  if (t->args) {
    task_args_destroy(t->args, t->group->is_kernel);
    free(t->args);
  }
  if (t->kstack) {
    kmem_page_free((void *)t->kstack, TASK_STACK_SIZE);
  }
  if (t->group) {
    if (!t->group->is_kernel) {
      utask_t *ut = (utask_t *)t;
      if (ut->pustack) {
        kmem_page_free((void *)ut->pustack, TASK_STACK_SIZE);
      }
    }
    task_group_remove(t);
  }
  if (t->name)
    free(t->name);
  vector_destroy(&t->joining);
  free(t);
}

static ktask_t *task_create_impl(const char *name, bool kernel,
                                 task_group_t *group, bool ref) {
  make_sure_schd_disabled();
  if (current && kernel && !current->group->is_kernel) {
    printf_color(CGA_COLOR_LIGHT_YELLOW,
                 "failed to create task %s(no permission to create kernel "
                 "level task)\n",
                 name);
    return 0; // 只有supervisor才能创造一个supervisor
  }
  if (group && (kernel != group->is_kernel)) {
    printf_color(
        CGA_COLOR_LIGHT_YELLOW,
        "failed to create task %s(group && (kernel != group->is_kernel))\n",
        name);
    return 0;
  }
  if (kernel) {
    // 如果是内核级
    if (group) {
      // 如果指定了线程组，那么必须是idle所在的那个组
      if (task_find(1)->group != group) {
        printf_color(CGA_COLOR_LIGHT_YELLOW,
                     "failed to create task %s(task_find(1)->group != group)\n",
                     name);
        return 0;
      }
    } else {
      // 如果没有指定线程组（意思是要创建一个），那么确保这是首个内核线程（idle）
      assert(task_find(1) == 0);
    }
  }

  bool group_created = false;
  if (!group) {
    // 创建一个group
    group = task_group_create(kernel);
    if (!group) {
      printf_color(
          CGA_COLOR_LIGHT_YELLOW,
          "failed to create task %s(unable to create new task group)\n", name);
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
    printf_color(
        CGA_COLOR_LIGHT_YELLOW,
        "failed to create task %s(unable .to create new task struct)\n", name);
    return 0;
  }
  if (task_inited == TASK_INITED_MAGIC) {
    new_task->tslice = task_current()->tslice;
  }

  // 内核栈
  new_task->kstack = (uintptr_t)kmem_page_alloc(TASK_STACK_SIZE);
  if (!new_task->kstack) {
    task_destroy(new_task);
    printf_color(
        CGA_COLOR_LIGHT_YELLOW,
        "failed to create task %s(unable to allocate stack for kernel)\n",
        name);
    return 0;
  }

  list_init(&new_task->group_head);
  new_task->state = CREATED;
  new_task->name = malloc(strlen(name) + 1);
  strcpy(new_task->name, name);
  // 生成id
  new_task->id = kernel_object_new(KERNEL_OBJECT_TASK, new_task);
  // 加入group
  task_group_add(group, new_task);
  // 自己引用自己
  kernel_object_ref(new_task->group, new_task->id);
  // 如果调用者有要求，就让现在的线程引用正在创建的线程
  if (ref)
    kernel_object_ref(task_current()->group, new_task->id);
  // 初始化joining
  vector_init(&new_task->joining, sizeof(pid_t), NULL);

  for (int i = SIGMIN; i <= SIGMAX; i++) {
    new_task->signal_callback[i - 1] = (uintptr_t)default_signal_handler;
  }

  if (task_inited == TASK_INITED_MAGIC) {
    // mutex_create和cv_create里默认会用当前线程去引用住那些kernel_obj
    // 但是如果任务系统没有初始化的时候，也就是在创建idle进程时，这就会出问题
    new_task->signal_seq_mut = mutex_create();
    new_task->signal_seq_fire_cv = condition_variable_create();
    kernel_object_ref(new_task->group, new_task->signal_seq_mut);
    kernel_object_ref(new_task->group, new_task->signal_seq_fire_cv);
    // 当前正运行的线程组取消引用
    if (task_current()->group != new_task->group) {
      kernel_object_unref(task_current()->group, new_task->signal_seq_mut,
                          true);
      kernel_object_unref(task_current()->group, new_task->signal_seq_fire_cv,
                          true);
    }
  }

#ifndef NDEBUG
  new_task->debug_current_syscall = 0;
#endif
  return new_task;
}

void kernel_task_entry();
void user_task_entry();

// 切换寄存器
// save指示是否要保存
void switch_to(bool save, void *from, void *to);

// 搜索task
ktask_t *task_find(pid_t pid) {
  SMART_CRITICAL_REGION
  ktask_t key;
  key.id = pid;
  void *ret = avl_tree_find(&tasks, &key);
  return ret;
}

// 显示系统中所有task
void task_display() {
  SMART_CRITICAL_REGION
  terminal_fgcolor(CGA_COLOR_LIGHT_CYAN);
  printf("\n\nTask Display   Current: %s", current->name);
  printf("\n*******************************************************************"
         "******\n");
  for (ktask_t *p = avl_tree_first(&tasks); p != 0;
       p = avl_tree_next(&tasks, p)) {
    printf(
        "State:%s ID:%d K:%s FG:%s Group:0x%08llx TS:%lld Name:%s\n",
        task_state_str(p->state), (int)p->id, p->group->is_kernel ? "T" : "F",
        p->id ==
                (shell_ready()
                     ? shell_fg()
                     : /*shell没有ready，确保此括号的值绝对不会是p->id*/ p->id +
                           1)
            ? "T"
            : "F",
        (int64_t)(uint64_t)(uintptr_t)p->group, p->tslice, p->name);
  }

  printf("*********************************************************************"
         "****\n\n");
  terminal_default_color();
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
  __unreachable;
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
        kernel_object_unref(p->group, p->id, true);
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
  if (t && t->state != EXITED) {
    if (allow_idle || t->id != 1) {
      if (force || t->tslice < task_current()->tslice) {
        // 如果t不如当前任务的时间片，就执行它
        {
          /*
           在切换前，还需确认当前使用的是内核栈。
           如果当前是用户栈，切换到别的进程页表时会无法访问当前用户栈，以至于出错

           比如，在用户程序刚开始执行但尚未通过系统调用切换到用户态前，
           如果出现时钟中断，就会以用户栈走到这个地方，最后出错

           或者甚至是用户进程完成系统调用，已离开criticalregion但还未切换回ring3时也会出错
          */
          uint32_t esp;
          resp(&esp);
          if (esp >= ((utask_t *)task_current())->vustack &&
              esp < ((utask_t *)task_current())->vustack +
                        TASK_STACK_SIZE * 4096) {
            // 为了解决这问题，如果发现现在是是用户栈，那么这次不switch
            return false;
          } else if (!(esp >= task_current()->kstack &&
                       esp < task_current()->kstack + TASK_STACK_SIZE * 4096)) {
            // 不是用户栈，但竟然也不是内核栈
            abort();
          }
          // 是内核栈
        }
        // 这里参数enable_schd总是为true使得系统最终总会开启抢占
        task_switch(t, true, tostate, false);
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
    task_schd(false, false, YIELDED);
    task_preemptive_set(false);
#ifdef VERBOSE
    // printf_color(CGA_COLOR_LIGHT_YELLOW, "idle: back\n");
    // kernel_object_print();
    // task_display();
#endif

  HANDLE_EXITED:
    for (ktask_t *t = avl_tree_first(&tasks); t != 0;) {
      if (t->state == EXITED) {
        // 处理join这个线程的线程
        for (uint32_t i = 0; i < vector_count(&t->joining); i++) {
          pid_t *id = vector_get(&t->joining, i);
          ktask_t *waiting_task = task_find(*id);
          assert(waiting_task->state == WAITING &&
                 waiting_task->wait_type == WAIT_JOIN &&
                 waiting_task->wait_ctx.join.id == t->id);
          // 告诉他们返回值
          waiting_task->wait_ctx.join.ret_val = t->ret_val;
          // 把他们放入调度队列
          waiting_task->state = YIELDED;
          ready_queue_put(waiting_task);
        }
        // 然后可以开始删除这个线程的PCB
        ktask_t *n = avl_tree_next(&tasks, t);
        if (t->ref_count == 1) {
          // 只有在没有别的线程引用时，才会删除
          avl_tree_remove(&tasks, t);
          kernel_object_unref(t->group, t->id, true);
        }
        t = n;
        continue;
      }
      t = avl_tree_next(&tasks, t);
    }
    for (ktask_t *t = avl_tree_first(&tasks); t != 0;
         t = avl_tree_next(&tasks, t)) {
      if (t->state == EXITED && t->ref_count == 1) {
        // 还有线程没有退出，需要重新走一遍上面的流程
        // 这种情况出现于一个线程（引用者）引用了另一个线程（被引用者）
        // 而到了上面的代码时，这两个线程都处于EXITED状态了，因此他们都会被
        // idle做退出处理。不幸的是，被引用者先于引用者进行处理，而idle看到被引用者
        // 有2个引用，因此没有进行推出处理，而当引用者取消对于被引用者的引用时，已经晚了
        // 因此在这里再检测一遍
        goto HANDLE_EXITED;
      }
    }

    hlt();
  }
}

// 这个volatile是为了task_disable_preemptive里的copy不被优化掉
static volatile bool task_preemptive = false;
bool task_preemptive_enabled() {
  bool copy = task_preemptive;
  return copy;
}
void task_preemptive_set(bool val) { task_preemptive = val; }

// 当前进程
ktask_t *task_current() {
  return (ktask_t *)(uintptr_t)atomic_load((uint32_t *)&current);
}

// 创建user task
// entry是开始执行的虚拟地址，arg是指向函数参数的虚拟地址
// 若program为0，则program_size将被忽略。program和group之间有且只有一个为非0，这意味着只有创建进程时才能指定program
pid_t task_create_user(void *program, uint32_t program_size, const char *name,
                       task_group_t *group, uintptr_t entry, bool ref,
                       struct task_args *args) {
  // FIXME 根据program_size去看底下elf处理时候有没有越界
  UNUSED(program_size);
  if (!(((program != 0) || (group != 0)) && ((program != 0) != (group != 0)))) {
    printf_color(CGA_COLOR_LIGHT_YELLOW,
                 "failed to create task %s(bad parameter)\n", name);
    return 0;
  }
  // mdzz
  bool is_first = program ? true : false;

  // 只有is_first时才能检测程序入口
  assert((entry == DEFAULT_ENTRY) == is_first);

  SMART_CRITICAL_REGION
  utask_t *new_task = (utask_t *)task_create_impl(name, false, group, ref);
  if (!new_task) {
    printf_color(CGA_COLOR_LIGHT_YELLOW,
                 "failed to create task %s(task_create_impl failed)\n", name);
    return 0;
  }
  group = new_task->base.group;
  // 用户栈
  new_task->pustack = (uintptr_t)kmem_page_alloc(TASK_STACK_SIZE);
  if (!new_task->pustack) {
    // 在task_create_impl之后失败的情况，不能调用task_destroy，因为此时它已经是一个kernel
    // object了， 必须通过kernel object的方式来删除
    kernel_object_unref(new_task->base.group, new_task->base.id, true);
    if (ref)
      kernel_object_unref(task_current()->group, new_task->base.id, true);
    printf_color(CGA_COLOR_LIGHT_YELLOW,
                 "failed to create task %s(unable to allocate user stack)\n",
                 name);
    return 0;
  }
  // 如果这是进程中首个线程，需要设置这个进程的虚拟内存
  if (is_first) {
    // 设置这个新group的虚拟内存
    group->vm = virtual_memory_create();
    if (!group->vm) {
      // 在task_create_impl之后失败的情况，不能调用task_destroy，因为此时它已经是一个kernel
      // object了， 必须通过kernel object的方式来删除
      kernel_object_unref(new_task->base.group, new_task->base.id, true);
      if (ref)
        kernel_object_unref(task_current()->group, new_task->base.id, true);
      printf_color(CGA_COLOR_LIGHT_YELLOW,
                   "failed to create task %s(virtual_memory_create failed)\n",
                   name);
      return 0;
    }
    // 存放程序映像的虚拟内存
    if (program) {
      if (new_task->base.group->program) {
        abort();
      }
      new_task->base.group->program =
          aligned_alloc(1, ROUNDUP(program_size, _4K));
      if (!new_task->base.group->program) {
        // 在task_create_impl之后失败的情况，不能调用task_destroy，因为此时它已经是一个kernel
        // object了， 必须通过kernel object的方式来删除
        kernel_object_unref(new_task->base.group, new_task->base.id, true);
        if (ref)
          kernel_object_unref(task_current()->group, new_task->base.id, true);
        printf_color(CGA_COLOR_LIGHT_YELLOW,
                     "failed to create task %s(unable to allocate memory for "
                     "program binary)\n",
                     name);
        return 0;
      }
      memset(new_task->base.group->program, 0, ROUNDUP(program_size, _4K));
      // 读elf
      struct elfhdr *elf_header = program;
      if (elf_header->e_magic != ELF_MAGIC) {
        // 在task_create_impl之后失败的情况，不能调用task_destroy，因为此时它已经是一个kernel
        // object了， 必须通过kernel object的方式来删除
        kernel_object_unref(new_task->base.group, new_task->base.id, true);
        if (ref)
          kernel_object_unref(task_current()->group, new_task->base.id, true);
        printf_color(
            CGA_COLOR_LIGHT_YELLOW,
            "failed to create task %s(program binary is not in ELF format)\n",
            name);
        return 0;
      }
      // FIXME 这里data segment和code seg要分开map
      // data给W权限，而code不给
      // 现在code因为和data一起map，所以整个区是用户可写的，这样不鲁棒
      struct proghdr *program_header, *ph_end;
      // load each program segment (ignores ph flags)
      program_header =
          (struct proghdr *)((uintptr_t)elf_header + elf_header->e_phoff);
      ph_end = program_header + elf_header->e_phnum;
      for (; program_header < ph_end; program_header++) {
        // FIXME 检查越界
        memcpy(new_task->base.group->program +
                   (program_header->p_va - USER_CODE_BEGIN),
               program + program_header->p_offset, program_header->p_filesz);
      }
      if (entry == DEFAULT_ENTRY) {
        entry = (uintptr_t)elf_header->e_entry;
      }
    } else {
      if (entry == DEFAULT_ENTRY) {
        abort();
      }
    }

    _Alignas(4096) uint32_t copy_pd[1024];
    {
      // 按照内核页表的样子建立vm，因为用户程序的vm也需要map进内核
      // 但是不能把kernal_pd的map区域也映射了，因为每个进程的map区域都是独立的
      extern uint32_t kernel_pd[];
      // 所以就需要先拷贝kernel_pd
      memcpy(copy_pd, kernel_pd, 4096);
      // 然后把拷贝里的map区域全部设0
      // 其实就是把map_region_vaddr的页一直到最后一页全部设0
      // (虽然其实map区域并不包含最后一个4k页，这也是为什么
      // map_region_size/4M != 1024-map_region_vaddr/4M)
      memset(&copy_pd[map_region_vaddr / _4M], 0,
             (1024 - map_region_vaddr / _4M) * 4);
    }
    virtual_memory_clone(new_task->base.group->vm, copy_pd, UKERNEL);
    // 把new_task->program映射到128MB的地方
    struct virtual_memory_area *vma = virtual_memory_alloc(
        new_task->base.group->vm, USER_CODE_BEGIN, ROUNDUP(program_size, _4K),
        PTE_P | PTE_W | PTE_U, UCODE, false);
    assert(vma);
    virtual_memory_map(new_task->base.group->vm, vma, USER_CODE_BEGIN,
                       ROUNDUP(program_size, _4K),
                       V2P((uintptr_t)new_task->base.group->program));
  }
  // 在虚拟内存中的用户栈
  new_task->vustack = virtual_memory_find_fit(
      new_task->base.group->vm, _4K * TASK_STACK_SIZE, USER_SPACE_BEGIN,
      USER_CODE_BEGIN, PTE_P | PTE_W | PTE_U, USTACK);
  struct virtual_memory_area *vma = virtual_memory_alloc(
      new_task->base.group->vm, new_task->vustack, _4K * TASK_STACK_SIZE,
      PTE_P | PTE_W | PTE_U, USTACK, false);
  assert(vma);
  virtual_memory_map(new_task->base.group->vm, vma, new_task->vustack,
                     _4K * TASK_STACK_SIZE, V2P(new_task->pustack));

  // 设置上下文和内核栈
  memset(&new_task->base.regs, 0, sizeof(struct gp_registers));
  new_task->base.regs.eip = (uint32_t)(uintptr_t)user_task_entry;
  new_task->base.regs.ebp = 0;
  uintptr_t kstack_top = new_task->base.kstack + _4K * TASK_STACK_SIZE;
  new_task->base.regs.esp = kstack_top - sizeof(void *) * 4;
  // 用户代码入口
  *(uintptr_t *)(new_task->base.regs.esp + 12) = entry;
  // 用户栈
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
                    new_task->base.group->vm, true,
                    new_task->base.group->vm_mutex);
    }
    task_args_pack(new_task->base.args, new_task->base.group->vm, true,
                   new_task->base.group->vm_mutex);
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

#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "task %lld(%s) created by %lld(%s)\n",
               (int64_t)new_task->base.id, new_task->base.name,
               (int64_t)task_current()->id, task_current()->name);
#endif

#ifdef VERBOSE
  virtual_memory_print(new_task->base.group->vm);
#endif

  return new_task->base.id;
}

// 创建内核线程
pid_t task_create_kernel(int (*func)(int, char **), const char *name, bool ref,
                         struct task_args *args) {
  SMART_CRITICAL_REGION
  ktask_t *schd = task_find(1);
  ktask_t *new_task = task_create_impl(name, true, schd->group, ref);
  if (!new_task) {
    printf_color(CGA_COLOR_LIGHT_YELLOW,
                 "failed to create task %s(task_create_impl failed)\n", name);
    return 0;
  }

  // 在task_destroy时候销毁args
  if (args) {
    task_args_pack(args, 0, false, 0);
    new_task->args = args;
  } else {
    new_task->args = 0;
  }

  // 设置上下文和内核栈
  memset(&new_task->regs, 0, sizeof(struct gp_registers));
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
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "task %lld(%s) created by %lld(%s)\n",
               (int64_t)new_task->id, new_task->name,
               (int64_t)task_current()->id, task_current()->name);
#endif
  return new_task->id;
}

// 等待进程结束
bool task_join(pid_t pid, int32_t *ret_val) {
  SMART_CRITICAL_REGION
  if (pid == task_current()->id) {
    abort();
  }
  ktask_t *task = task_find(pid);
  if (!task)
    return false;
  if (task->state == EXITED) {
    // 场景是本线程此前已经ref了该线程，所以该线程退出之后还没有被销毁
    // join之后，需要本线程手动unref，不然那个线程的结构就没人释放了
    // 当然，你也可以等本线程退出时候自动unref他们
    if (ret_val)
      *ret_val = task->ret_val;
    return true;
  } else {
    SMART_NOINT_REGION
    task_current()->wait_type = WAIT_JOIN;
    task_current()->wait_ctx.join.id = pid;
    // 把本线程的值加到那个线程的joining里
    uint32_t idx = vector_add(&task->joining, &task_current()->id);
    // 开始等待那个线程退出
    task_current()->tslice++;
    task_schd(true, true, WAITING);
    // 等再回来的时候，那个线程的返回值已经被存给我们了
    if (ret_val)
      *ret_val = task_current()->wait_ctx.join.ret_val;

    // 还需要把我们从joining中删除
    // 这里有一种可能，别的线程已经修改过task->joining了，所以我们要做相应的处理
    if (task->joining.count >= idx + 1 &&
        *(pid_t *)vector_get(&task->joining, idx) == task_current()->id) {
      vector_remove(&task->joining, idx);
    } else {
      // 这看起来相当蠢，但我感觉这里用线性表足够了
      for (uint32_t i = 0; i < vector_count(&task->joining); i++) {
        if (*(pid_t *)vector_get(&task->joining, i) == task_current()->id)
          vector_remove(&task->joining, i);
      }
    }
  }
  return true;
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
    task_current()->wait_type = WAIT_SLEEP;
    task_current()->wait_ctx.sleep.after =
        clock_get_ticks() * TICK_TIME_MS + millisecond;
    task_current()->tslice++;
    task_schd(true, true, WAITING);
    assert(clock_get_ticks() * TICK_TIME_MS >=
           task_current()->wait_ctx.sleep.after);
  }
}

static void task_quit(int32_t ret) {
  SMART_CRITICAL_REGION
#ifdef VERBOSE
  printf_color(CGA_COLOR_LIGHT_YELLOW, "task %lld returned %d\n",
               (int64_t)task_current()->id, ret);
#endif
  assert(task_current()->ref_count != 0);
  // 切到idle去开始删除线程
  disable_interrupt();
  // 找到idle task，它的pid是1
  ktask_t *idle = task_find(1);
  // 切换到idle
  task_current()->ret_val = ret;
  task_switch(idle, task_preemptive_enabled(), EXITED, false);
  abort();
  __unreachable;
}

// 退出当前进程
void task_exit(int32_t ret) {
  if (ret < 0) {
    panic("task_exit ret < 0");
  }
  task_quit(ret);
}

static const char *task_terminate_reason_str(int32_t number) {
  if (number == -1)
    return "ABORT";
  if (number == -2)
    return "BAD ACCESS";
  if (number == -3)
    return "INVALID ARGUMENT";
  if (number == -4)
    return "QUIT ABNORMALLY";
  panic("task_terminate_reason_str");
  __unreachable;
}

// 非正常中止
void task_terminate(int32_t ret) {
  printf_color(
      CGA_COLOR_RED,
      "task %lld(%s) terminated with err code %d(%s), all threads within the "
      "same "
      "thread group will be killed\n",
      (int64_t)task_current()->id, task_current()->name, ret,
      task_terminate_reason_str(ret));

  if (task_current()->id == 1) {
    panic("idle called terminate!");
  }
  if (ret >= 0) {
    // 非正常退出返回值应该是负数
    panic("task_terminate ret >= 0");
  }

  // 释放该进程持有的内核对象是通过引用计数的内核对象系统自动完成的
  // 释放该进程的内存是在销毁线程组时进行的
  disable_interrupt();

  // 把同vm的其他线程全部设为退出状态
  for (list_entry_t *p = list_next(&task_current()->group->tasks);
       p != &task_current()->group->tasks; p = list_next(p)) {
    ktask_t *task = task_group_head_retrieve(p);
    if (task != task_current()) {
      // copy from task_quit
      printf_color(CGA_COLOR_RED, "task %lld killed\n", (int64_t)task->id, ret);
      assert(task->ref_count != 0);
      task->state = EXITED;
      task->ret_val = ret;
    }
  }
  terminal_default_color();
  // 释放该进程持有的锁
  // kernel_object_release_mutexs(task_current()->group);
  task_quit(ret);
}

// 切换到另一个task
void task_switch(ktask_t *next, bool enable_schd,
                 enum task_state current_new_state, bool handle_signal) {
  SMART_NOINT_REGION
  assert(current && next);
  assert(next &&
         (handle_signal || (next->state == CREATED || next->state == YIELDED)));
  if (next == current) {
    panic("task_switch: next == current");
  }

  {
    uint32_t esp;
    resp(&esp);
    if (!(esp >= task_current()->kstack &&
          esp < task_current()->kstack + TASK_STACK_SIZE * 4096)) {
      // 当前不是内核栈
      panic("task_switch: not on kernel stack");
    }
  }

  ktask_t *const prev = current;
  current->state = current_new_state;
  if (current->group != next->group) {
    // 不是同进程切换
    // 需要切换页表
    union {
      struct CR3 cr3;
      uintptr_t val;
    } cr3;
    cr3.val = 0;
    set_cr3(&cr3.cr3, V2P((uintptr_t)next->group->vm->page_directory), false,
            false);
    lcr3(cr3.val);
  }

  if (!next->group->is_kernel) {
    // 如果下一个线程是用户，加载该线程的内核栈到esp0
    // 这样该进程在遇到中断而陷入内核态时会使用esp0指示的栈
    load_esp0(next->kstack + _4K * TASK_STACK_SIZE);
  } else {
    load_esp0(0);
  }

  if (!handle_signal) {
    // 处理信号时，保留原先的state
    next->state = RUNNING;
  }
  current = next;
  current_vm = next->group->vm;
  // next在ready队列里面(若当前为了处理信号而调入next，则此项可以为假)，prev不在队列里面
  assert(handle_signal || (next->ready_queue_head.next != 0 &&
                           next->ready_queue_head.prev != 0));
  assert(prev->ready_queue_head.next == 0 || prev->ready_queue_head.prev == 0);

  if (!handle_signal) {
    // 从队列移除next
    // 如果是处理信号，不需要这样做，因为处理信号的调入，不是正常的调入，执行完signal
    // handler就会交出
    list_del(&next->ready_queue_head);
    next->ready_queue_head.next = 0;
    next->ready_queue_head.prev = 0;
  }
  // 向队列加入prev
  if (prev->state == YIELDED) {
    ready_queue_put(prev);
  }
  task_preemptive_set(enable_schd);

  // 保存cr2
  if (prev->state != EXITED) {
    uintptr_t cr2 = rcr2();
    if (cr2) {
      prev->cr2 = cr2;
    }
  }

  // terminal_fgcolor(CGA_COLOR_BLUE);
  // printf("%s(id=%lld) -> %s(id=%lld)\n", prev->name, (int64_t)prev->id,
  //        next->name, (int64_t)next->id);
  // terminal_default_color();

  // 切换寄存器，包括eip、esp和ebp
  switch_to(prev->state != EXITED, &prev->regs, &next->regs);
  // 本线程(prev)重新开始执行了
  // 这里有两种情况
  // 1.本线程已经是YIELD/CREATED这样的可执行状态
  // 2.本线程是WAITING状态
  // 情况1是正常情况，情况2是因为有信号需要处理
  // 对于情况2，处理完信号之后继续等待
  assert((reflags() & FL_IF) == 0);
  assert(prev->state != EXITED);

  // 恢复cr2
  if (prev->cr2) {
    lcr2(prev->cr2);
  }

  // 任务被换入，此时执行信号模块
  // 但是idle进程是例外
  if (prev->id != 1) {
    signal_handle_on_task_schd_in();
  }
  if (prev->state != RUNNING) {
    // 说明本次换入只是在执行信号处理
    // 现在已经处理好信号，那么我们交出控制权
    // 本次处理信号不算时间片
    task_switch(task_find(1), task_preemptive_enabled(), prev->state, false);
  }
}

// 初始化任务系统
void task_init() {
  assert(TASK_TIME_SLICE_MS >= TICK_TIME_MS &&
         TASK_TIME_SLICE_MS % TICK_TIME_MS == 0);
  avl_tree_init(&tasks, compare_task, sizeof(ktask_t), 0);
  list_init(&ready_queue);

  // 将当前的上下文设置为第一个任务
  ktask_t *init = task_create_impl("idle", true, 0, false);
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

void task_post_init() {
  task_current()->group->vm_mutex = mutex_create();
  task_current()->group->input_buffer_mutex = mutex_create();
  task_current()->group->input_buffer_cv = condition_variable_create();
}