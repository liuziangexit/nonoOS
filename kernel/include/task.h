#ifndef __KERNEL_TASK_H__
#define __KERNEL_TASK_H__
#include <interrupt.h>
#include <list.h>
#include <stdbool.h>
#include <stdint.h>

enum task_state {
  CREATED, //
  YIELDED, //
  RUNNING
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

//内核线程
struct kernel_task {
  list_entry_t list_head;
  bool kernel; //是否内核权限
  enum task_state state;
  pid_t id;
  const char *name;
  struct kernel_task *parent; //父进程
  uintptr_t kstack;           //内核栈
  struct registers regs;      //寄存器
};
typedef struct kernel_task kernel_task_t;

/*
用户线程的地址空间：
[0x100000, 0x100000+程序大小): 代码和数据（text, rodata, data, bss）
[0xFFFFFFFF-栈大小, 0xFFFFFFFF): 用户栈
用户栈和代码之间的空间是heap，用来malloc或者mmap
*/

//用户线程
struct user_task {
  kernel_task_t base;
  uintptr_t pgd; //页目录
  struct trapframe tf;
  uintptr_t ustack; //用户栈
};
typedef struct user_task user_task_t;

void task_display();

//初始化进程系统
void task_init();

//当前进程
pid_t task_current();

//创建进程
pid_t task_create(void (*func)(void *), void *arg, const char *name,
                  bool kernel);

//等待进程结束
void task_join(pid_t pid);

//放弃当前进程时间片
void task_yield();

//将当前进程挂起一段时间
void task_sleep(uint64_t millisecond);

//退出当前进程
// aka exit
void task_exit();

//切换到另一个task
void task_switch(pid_t);

#endif
