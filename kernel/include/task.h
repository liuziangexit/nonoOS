#ifndef __KERNEL_TASK_H__
#define __KERNEL_TASK_H__
#include <interrupt.h>
#include <list.h>
#include <stdbool.h>
#include <stdint.h>

enum task_state {
  CREATED, //
  YIELDED, //
  RUNNING, //
  EXITED
};

const char *task_state_str(enum task_state);

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

typedef uint32_t pid_t;

// task group中的tasks共享同一个地址空间
// 每个task都必须属于一个task group，每个task group至少要有一个task
struct task_group {
  list_entry_t tasks;
  uint32_t task_cnt;
  bool is_kernel; //是否内核权限
  uintptr_t pgd;  //页目录
};
typedef struct task_group task_group_t;

//内核task
struct ktask {
  list_entry_t global_head;
  list_entry_t group_head;
  task_group_t *group;
  enum task_state state;
  pid_t id;
  const char *name;
  struct ktask *parent;  //父进程
  uintptr_t kstack;      //内核栈
  struct registers regs; //寄存器
};
typedef struct ktask ktask_t;

/*
用户线程的地址空间：
[0x100000, 0x100000+程序大小): 代码和数据（text, rodata, data, bss）
[0xFFFFFFFF-栈大小, 0xFFFFFFFF): 用户栈
用户栈和代码之间的空间是heap，用来malloc或者mmap
*/

//用户线程
struct utask {
  ktask_t base;
  struct trapframe tf;
  uintptr_t ustack; //用户栈
};
typedef struct utask utask_t;

ktask_t *task_find(pid_t pid);

void task_display();

void task_clean();

//初始化task系统
void task_init();

//当前task
pid_t task_current();

//创建task
pid_t task_create(void (*func)(void *), void *arg, const char *name,
                  bool kernel, task_group_t *group);

//等待task结束
void task_join(pid_t pid);

//放弃当前task时间片
void task_yield();

//将当前task挂起一段时间
void task_sleep(uint64_t millisecond);

//退出当前task
// aka exit
void task_exit();

//切换到另一个task
void task_switch(pid_t);

#endif
