#ifndef __KERNEL_TASK_H__
#define __KERNEL_TASK_H__
#include <list.h>
#include <stdbool.h>
#include <stdint.h>

enum task_state {
  CREATED,    //
  YIELDED,    //
  WAITING_IO, //
  RUNNING,    //
  STOPPED
};

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

struct context {
  struct registers regs; //寄存器
};

typedef uint32_t pid_t;

struct task {
  list_entry_t list_head;
  bool supervisor; //内核权限
  enum task_state state;
  pid_t id;
  struct task *parent; //父进程
  uintptr_t ustack;    //用户栈
  uintptr_t kstack;    //内核栈(系统调用时)
  uintptr_t pgd;       //页目录地址
  struct context ctx;  //上下文
};
typedef struct task task_t;

//初始化进程系统
void task_init();

//当前进程
pid_t task_current();

//创建进程
pid_t task_create(bool supervisor, size_t stack_size);

//等待进程结束
void task_join(pid_t pid);

//放弃当前进程时间片
void task_yield();

//将当前进程挂起一段时间
void task_sleep(uint64_t millisecond);

//退出当前进程
// aka exit
void task_exit();

#endif
