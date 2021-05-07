#ifndef __KERNEL_TASK_H__
#define __KERNEL_TASK_H__
#include <interrupt.h>
#include <list.h>
#include <stdbool.h>
#include <stdint.h>
#include <virtual_memory.h>

//这个指的是4k页数
#define TASK_STACK_SIZE 1024

enum task_state {
  CREATED, //已创建
  YIELDED, //被调走
  // WAITING, //正在等待同步或IO
  RUNNING, //运行中
  EXITED   //已退出
};

const char *task_state_str(enum task_state);

typedef uint32_t pid_t;

// task group中的tasks共享同一个地址空间
// 每个task都必须属于一个task group，每个task group至少要有一个task
struct task_group {
  list_entry_t tasks;
  uint32_t task_cnt;
  bool is_kernel;            //是否内核权限
  struct virtual_memory *vm; //虚拟内存管理
};
typedef struct task_group task_group_t;

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

//内核task
struct ktask {
  struct avl_node global_head;
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
  uintptr_t pustack; //用户栈
  uintptr_t vustack; //用户栈的虚拟地址
  void *program;     //程序映像拷贝
};
typedef struct utask utask_t;

//内部函数

ktask_t *task_find(pid_t pid);

void task_display();

void task_clean();

//对外接口

//对kernel接口
//初始化task系统
void task_init();

//对kernel接口
//开始调度，此函数不会返回
void task_schd();

//对kernel接口
//对task接口
//当前task
pid_t task_current();

//对kernel接口
//创建kernel task
pid_t task_create_kernel(void (*func)(void *), void *arg, const char *name);

//对kernel接口
//创建user task
#define DETECT_ENTRY 0xC0000000
pid_t task_create_user(void *program, uint32_t program_size, const char *name,
                       task_group_t *group, uintptr_t entry, int arg_count,
                       ...);

//对kernel接口
//对task接口
//等待task结束
void task_join(pid_t pid);

//对task接口
//放弃当前task时间片
void task_yield();

//对task接口
//将当前task挂起一段时间
void task_sleep(uint64_t millisecond);

//对task接口
//退出当前task
// aka exit
void task_exit();

//对kernel接口
//切换到另一个task
void task_switch(ktask_t *);

#endif
