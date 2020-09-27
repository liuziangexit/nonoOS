#ifndef __KERNEL_PROC_H__
#define __KERNEL_PROC_H__
#include <list.h>
#include <stdint.h>

enum task_state {
  CREATED,    //
  YIELDED,    //
  WAITING_IO, //
  RUNNING,    //
  STOPPED
};

struct task {
  list_entry_t list_head;
  enum task_state state;
  uint32_t id;
  uintptr_t stack;
};

#endif
