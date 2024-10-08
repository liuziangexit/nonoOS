#ifndef __KERNEL_TASK_H__
#define __KERNEL_TASK_H__
#include <interrupt.h>
#include <list.h>
#include <ring_buffer.h>
#include <signal_def.h>
#include <stdbool.h>
#include <stdint.h>
#include <vector.h>
#include <virtual_memory.h>

/*
用户程序的内存布局:
[128MB, 128MB + program size)代码
[128MB + program size, 3G - 512MB)malloc区
[3G - 512MB, 3G - 512MB + STACK_SIZE)用户栈
实现已经改变，以上描述不准确

*/

// 这个指的是4k页数
#define TASK_STACK_SIZE 1024
// 用户代码的虚拟地址
#define USER_SPACE_BEGIN (0x0)
#define USER_CODE_BEGIN (0x8000000)
#define USER_SPACE_END (0xC0000000)
// 时间片
#define TASK_TIME_SLICE_MS 10

#define TASK_INITED_MAGIC 9863479
extern uint32_t task_inited;

typedef uint32_t pid_t;

enum task_state {
  CREATED, // 已创建
  YIELDED, // 被调走
  WAITING, // 等待资源
  RUNNING, // 运行中
  EXITED,  // 已退出，但还不可删除
};

enum task_wait_type { WAIT_SLEEP, WAIT_JOIN, WAIT_MUTEX_TIMED, WAIT_CV_TIMED };

struct sleep_ctx {
  uint64_t after; // 当ticks * TICK_TIME_MS >= after，就等到了
};
struct join_ctx {
  pid_t id;        // 要等待的线程ID
  int32_t ret_val; // 该线程的返回值
};
struct mutex_ctx {
  uint32_t mutex_id; // 等待的mutex号
  uint64_t after;    // 同sleep_ctx，这个是timedlock时用的，平时为0
  bool timeout;      // 由时钟中断设置，指示是否因为超时返回
};
struct cv_ctx {
  uint32_t cv_id; // 等待的cv号
  uint64_t after; // 同sleep_ctx，这个是timedlock时用的，平时为0
  bool timeout;   // 由时钟中断设置，指示是否因为超时返回
};

union task_wait_ctx {
  struct sleep_ctx sleep;
  struct join_ctx join;
  struct mutex_ctx mutex;
  struct cv_ctx cv;
};

const char *task_state_str(enum task_state);

#define TASK_INPUT_BUFFER_LEN 512

// task group中的tasks共享同一个地址空间
// 每个task都必须属于一个task group，每个task group至少要有一个task
// task_group的生命周期通过引用计数来控制。不过它不是一个kernel
// object，它的引用计数是自己实现的
struct task_group {
  list_entry_t tasks;
  uint32_t input_buffer_mutex;
  uint32_t input_buffer_cv;
  struct ring_buffer input_buffer;
  uint32_t task_cnt;
  bool is_kernel;            // 是否内核权限
  struct virtual_memory *vm; // 虚拟内存管理
  uint32_t vm_mutex;
  void *program;                  // 程序映像拷贝
  struct avl_tree kernel_objects; // 线程引用的内核对象
};
typedef struct task_group task_group_t;

// 表示argc+argv
struct task_args {
  // argc
  uint32_t cnt;
  // argv数组的物理地址
  uintptr_t packed;
  // argv数组的虚拟地址
  uintptr_t vpacked;
  // 许多struct task_arg在这里串起来
  list_entry_t args;
};

// 代表单个char*参数
struct task_arg {
  list_entry_t head;
  uint32_t strlen;
  uintptr_t vdata;
  uintptr_t data;
};

void task_args_init(struct task_args *dst);
void task_args_add(struct task_args *dst, const char *str,
                   struct virtual_memory *vm, bool use_umalloc,
                   uint32_t vm_mut);
void task_args_destroy(struct task_args *dst, bool free_data);

struct gp_registers {
  uint32_t eip;
  uint32_t esp;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
  uint32_t esi;
  uint32_t edi;
  uint32_t ebp;
};

struct kern_obj_id {
  struct avl_node head;
  uint32_t id;
};

/*
CAUTION!
task_group_head_retrieve
tree
依赖于此类型的内存布局
*/
// 内核task
struct ktask {
  struct avl_node global_head;
  list_entry_t ready_queue_head;
  list_entry_t group_head;
  // 内核对象引用计数
  // 此线程被别的线程join时会加引用
  uint32_t ref_count;
  // 所属线程组
  task_group_t *group;
  // 状态
  enum task_state state;
  pid_t id;
  char *name;
  // 内核栈
  uintptr_t kstack;
  // 寄存器
  struct gp_registers regs;
  // CR2
  uintptr_t cr2;
  // 命令行参数
  struct task_args *args;
  // 已使用的时间片
  uint64_t tslice;
  // 等待类型，只有在当前state==WAITING时此字段才有意义
  enum task_wait_type wait_type;
  // 等待上下文，只有在当前state==WAITING时此字段才有意义
  union task_wait_ctx wait_ctx;
  // 在等待本task退出的task
  vector_t joining;
  // 返回值
  int32_t ret_val;

  // 信号处理回调函数表
  // 在Mac上kill -l发现只有32个信号
  // 我们的系统肯定没有那么多信号，所以定义32个就足够了
  // 注意，因为SIG从1开始数，所以这里索引的方式是，signal_callback[sig - 1]
  // 当其中的值为0时，表示忽略此信号
  uintptr_t signal_callback[SIGMAX];

  // 这里是信号处理记录表
  // 当发射信号s时，signal_fire_seq[s]++
  // 此时signal_fin_seq[s]将会比signal_fire_seq[s]少1
  // 当完成信号处理后，signal_fin_seq[s]++，此时signal_fin_seq[s]与signal_fire_seq[s]相等，
  // 表示都处理完了，没有新的信号要处理了
  // 这使得我们可以支持某个信号在被处理之前，多次被发射的情形
  // 而且可以实现sigwait的功能
  unsigned char signal_fire_seq[SIGMAX];
  unsigned char signal_fin_seq[SIGMAX];
  uint32_t signal_seq_mut;
  uint32_t signal_seq_fire_cv;
#ifndef NDEBUG
  uint32_t debug_current_syscall;
#endif
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
};
typedef struct utask utask_t;

// 从组链表node获得对象
inline static ktask_t *task_group_head_retrieve(list_entry_t *head) {
  return (ktask_t *)(((void *)head) - sizeof(struct avl_node) -
                     sizeof(list_entry_t));
}

// 从ready链表node获得对象
inline static ktask_t *task_ready_queue_head_retrieve(list_entry_t *head) {
  return (ktask_t *)(((void *)head) - sizeof(struct avl_node));
}

// 内部函数
void ready_queue_put(ktask_t *t);

ktask_t *task_find(pid_t pid);

void task_display();

void task_clean();

void task_handle_wait();

// 对外接口

// 初始化task系统
void task_init();
void task_post_init();

// 寻找下一个可调度的task
bool task_schd(bool force, bool allow_idle, enum task_state tostate);
// 在没有task可调时，hlt
// 返回值指示是否切换了其他任务
void task_idle();

// 开关抢占式调度
bool task_preemptive_enabled();
void task_preemptive_set(bool val);

// 当前task
ktask_t *task_current();

// 创建内核线程
pid_t task_create_kernel(int (*func)(int, char **), const char *name, bool ref,
                         struct task_args *args);

// 创建user task
#define DEFAULT_ENTRY 0
pid_t task_create_user(void *program, uint32_t program_size, const char *name,
                       task_group_t *group, uintptr_t entry, bool ref,
                       struct task_args *args);

void task_destroy(ktask_t *t);

// 等待task结束
// 返回被等待task的返回值
bool task_join(pid_t pid, int32_t *ret_val);

// 放弃当前task时间片
void task_yield();

// 将当前task挂起一段时间
void task_sleep(uint64_t millisecond);

// main函数正常返回值
void task_exit(int32_t ret);

// 非正常退出

// 用户态abort被调用
#define TASK_TERMINATE_ABORT (-1)
// 访问内存异常
#define TASK_TERMINATE_BAD_ACCESS (-2)
// 系统调用参数非法
#define TASK_TERMINATE_INVALID_ARGUMENT (-3)
// 强制退出
#define TASK_TERMINATE_QUIT_ABNORMALLY (-4)
void task_terminate(int32_t ret);

// 切换到另一个task
void task_switch(ktask_t *next, bool enable_schd,
                 enum task_state current_new_state, bool handle_signal);

#endif
