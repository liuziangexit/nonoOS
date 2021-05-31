#ifndef __KERNEL_KERNEL_OBJECT_H__
#define __KERNEL_KERNEL_OBJECT_H__
#include "task.h"
#include <stdint.h>

enum _kernel_object_type {
  KERNEL_OBJECT_SHARED_MEMORY,
  KERNEL_OBJECT_MUTEX,
  KERNEL_OBJECT_CONDITION_VARIABLE
};
typedef enum _kernel_object_type kernel_object_type;

// type | pointer | ref_offset | dtor

uint32_t kernel_object_new_id(kernel_object_type t);
void kernel_object_ref(ktask_t *task, uint32_t kobj_id);
void kernel_object_unref(ktask_t *task, uint32_t kobj_id);

#endif