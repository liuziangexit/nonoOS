#ifndef __KERNEL_KERNEL_OBJECT_H__
#define __KERNEL_KERNEL_OBJECT_H__
#include "task.h"
#include <stdint.h>

/*
跨进程共享的资源都要统一以kernel_object的方式管理其生命周期以避免泄漏
本质上是引用计数
*/

enum _kernel_object_type {
  KERNEL_OBJECT_TASK,
  KERNEL_OBJECT_SHARED_MEMORY,
  KERNEL_OBJECT_MUTEX,
  KERNEL_OBJECT_CONDITION_VARIABLE
};
typedef enum _kernel_object_type kernel_object_type;

void kernel_object_init();
void *kernel_object_get(uint32_t id, bool unsafe);
uint32_t kernel_object_new(kernel_object_type t, void *obj,
                           bool auto_lifecycle);
// bool kernel_object_has_ref(task_group_t *group, uint32_t kobj_id);
bool kernel_object_ref(task_group_t *group, uint32_t kobj_id);
bool kernel_object_ref_safe(pid_t pid, uint32_t kobj_id);
void kernel_object_unref(task_group_t *group, uint32_t kobj_id,
                         bool remove_from_task_avl);
void kernel_object_print();
void kernel_object_release_mutexs(task_group_t *group);

#endif