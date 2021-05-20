#ifndef __KERNEL_TASK_H__
#define __KERNEL_TASK_H__
#include <interrupt.h>
#include <list.h>
#include <stdbool.h>
#include <stdint.h>
#include <virtual_memory.h>

/*
用户程序的内存布局:
[128MB, 128MB + program size)代码
[128MB + program size, 3G - 512MB)malloc区
[3G - 512MB, 3G - 512MB + STACK_SIZE)用户栈
*/

// 这个指的是4k页数
#define TASK_STACK_SIZE 1024
// 用户代码的虚拟地址
#define USER_CODE_BEGIN 0x8000000
// 用户栈的虚拟地址(3GB - 4K)
#define USER_STACK_BEGIN (0xC0000000 - TASK_STACK_SIZE * 4096)
// 时间片
#define TASK_TIME_SLICE_MS 10

#define TASK_INITED_MAGIC 9863479
extern uint32_t task_inited;

enum task_state {
  CREATED, // 已创建
  YIELDED, // 被调走
  // WAITING, // 正在等待同步或IO
  RUNNING, // 运行中
  EXITED   // 已退出
};

const char *task_state_str(enum task_state);

typedef uint32_t pid_t;

// task group中的tasks共享同一个地址空间
// 每个task都必须属于一个task group，每个task group至少要有一个task
struct task_group {
  list_entry_t tasks;
  uint32_t task_cnt;
  bool is_kernel;            // 是否内核权限
  struct virtual_memory *vm; // 虚拟内存管理
};
typedef struct task_group task_group_t;

struct task_args {
  uint32_t cnt;
  uintptr_t packed;
  uintptr_t vpacked;
  list_entry_t args;
};
struct task_arg {
  list_entry_t head;
  uint32_t strlen;
  uintptr_t vdata;
  uintptr_t data;
};

void task_args_init(struct task_args *dst);
void task_args_add(struct task_args *dst, const char *str,
                   struct virtual_memory *vm, bool use_umalloc);

struct registers {
  uint32_t eip;
  uint32_t esp;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
  uint32_t esi;
  uint32_t edi;
  uint32_t ebp;
};

/*
CAUTION!
task_group_head_retrieve
tree
依赖于此类型的内存布局
*/
#define DPRIOR_MAX 100
// 内核task
struct ktask {
  struct avl_node global_head;
  list_entry_t ready_queue_head;
  list_entry_t group_head;
  task_group_t *group;
  enum task_state state;
  pid_t id;
  const char *name;
  struct ktask *parent;   // 父进程
  uintptr_t kstack;       // 内核栈
  struct registers regs;  // 寄存器
  struct task_args *args; // 命令行参数
  /*
   动态优先级，数值越高优先级越高

   当本次周转时间(ctat)低于目标周转时间(etat)时，动态优先级下降，dynamic_priority-=(etat-ctat)*(priority>=0?(100+priority)/100的倒数:(100-priority)/100)
   当本次周转时间(ctat)高于目标周转时间(etat)时，动态优先级上升，dynamic_priority+=(ctat-etat)*(priority>=0?(100+priority)/100:(100-priority)/100的倒数)

   dynamic_priority的最大值和最小值分别为PRIOR_MAX和-PRIOR_MAX，
   这表明在最坏情况下，最低优先级任务的周转时间（不考虑上下文切换）也不会超过PRIOR_MAX*2*(task_cnt-1)
   (考虑一个任务具有最低dp也就是-DPRIOR_MAX，另有task_cnt-1个任务具有最高优先级也就是DPRIOR_MAX)
   */
  int32_t dynamic_priority; // [-DPRIOR_MAX, DPRIOR_MAX]
  int32_t priority; // 优先级是用来影响动态优先级的[-500, 500]，0是默认优先级
  uint64_t schd_out; // 上次被调走的时间(tick)
};
typedef struct ktask ktask_t;

/*
用户线程的地址空间：
[0x100000, 0x100000+程序大小): 代码和数据（text, rodata, data, bss）
[0xFFFFFFFF-栈大小, 0xFFFFFFFF): 用户栈
用户栈和代码之间的空间是heap，用来malloc或者mmap
*/

// 用户线程
struct utask {
  ktask_t base;
  uintptr_t pustack; // 用户栈
  uintptr_t vustack; // 用户栈的虚拟地址
  void *program;     // 程序映像拷贝
};
typedef struct utask utask_t;

// 内部函数

ktask_t *task_find(pid_t pid);

void task_display();

void task_clean();

// 对外接口

// 对kernel接口
// 初始化task系统
void task_init();

// 对kernel接口
// 寻找下一个可调度的task
bool task_schd();
// 在没有task可调时，hlt
// 返回值指示是否切换了其他任务
void task_idle();
// 关闭抢占式调度
extern bool task_preemptive;
void task_disable_preemptive();
void task_enable_preemptive();

// 对kernel接口
// 对task接口
// 当前task
ktask_t *task_current();

// 对kernel接口
// 创建内核线程
pid_t task_create_kernel(int (*func)(int, char **), const char *name,
                         struct task_args *args);

// 对kernel接口
// 创建user task
#define DEFAULT_ENTRY 0
pid_t task_create_user(void *program, uint32_t program_size, const char *name,
                       task_group_t *group, uintptr_t entry,
                       struct task_args *args);

// 对kernel接口
// 对task接口
// 等待task结束
// 返回被等待task的返回值
int32_t task_join(pid_t pid);

// 对task接口
// 放弃当前task时间片
void task_yield();

// 对task接口
// 将当前task挂起一段时间
void task_sleep(uint64_t millisecond);

// 对task接口
// 退出当前task
// aka exit
void task_exit(int32_t ret);

// 对kernel接口
// 切换到另一个task
void task_switch(ktask_t *);

#endif
