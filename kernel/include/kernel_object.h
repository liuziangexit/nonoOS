#ifndef __KERNEL_KERNEL_OBJECT_H__
#define __KERNEL_KERNEL_OBJECT_H__
#include "task.h"
#include <stdint.h>

/*
跨进程共享的资源都要统一以kernel_object的方式管理其生命周期以避免泄漏
本质上是引用计数
*/

enum _kernel_object_type {
  KERNEL_OBJECT_SHARED_MEMORY,
  //   KERNEL_OBJECT_MUTEX,
  //   KERNEL_OBJECT_CONDITION_VARIABLE
};
typedef enum _kernel_object_type kernel_object_type;

void kernel_object_init();
void *kernel_object_get(uint32_t id);
uint32_t kernel_object_new(kernel_object_type t, void *obj);
void kernel_object_delete(uint32_t id);
bool kernel_object_ref(ktask_t *task, uint32_t kobj_id);
bool kernel_object_ref_safe(pid_t pid, uint32_t kobj_id);
void kernel_object_unref(ktask_t *task, uint32_t kobj_id,
                         bool remove_from_task_avl);

#endif