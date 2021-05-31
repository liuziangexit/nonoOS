#include <atomic.h>
#include <avlmini.h>
#include <kernel_object.h>
#include <panic.h>
#include <sync.h>
#include <task.h>
#include <virtual_memory.h>

//#define VERBOSE

static uint32_t id_seq;
struct avl_tree id_tree;

struct id_ctx {
  struct avl_node head;
  uint32_t id;
  kernel_object_type type;
  void *object;
};

static int compare_id(const void *a, const void *b) {
  struct id_ctx *ta = (struct id_ctx *)a;
  struct id_ctx *tb = (struct id_ctx *)b;
  if (ta->id > tb->id) {
    return 1;
  }
  if (ta->id < tb->id) {
    return -1;
  }
  return 0;
}

static struct id_ctx *get_ctx(uint32_t id) {
  make_sure_schd_disabled();
  struct id_ctx find;
  find.id = id;
  SMART_CRITICAL_REGION
  return avl_tree_find(&id_tree, &find);
}

static uint32_t *get_counter(kernel_object_type t, void *obj) {
  switch (t) {
  case KERNEL_OBJECT_SHARED_MEMORY: {
    return (uint32_t *)(obj + 20);
  } break;
    //   case KERNEL_OBJECT_MUTEX: {
    //   } break;
    //   case KERNEL_OBJECT_CONDITION_VARIABLE: {
    //   } break;
  }
  panic("AHAHAH");
  __builtin_unreachable();
}

static void *get_dtor(kernel_object_type t) {
  switch (t) {
  case KERNEL_OBJECT_SHARED_MEMORY: {
    return shared_memory_destroy;
  } break;
    //   case KERNEL_OBJECT_MUTEX: {
    //   } break;
    //   case KERNEL_OBJECT_CONDITION_VARIABLE: {
    //   } break;
  }
  panic("AHAHAH");
  __builtin_unreachable();
}

void kernel_object_init() {
  avl_tree_init(&id_tree, compare_id, sizeof(struct id_ctx), 0);
}

void *kernel_object_get(uint32_t id) {
  make_sure_schd_disabled();
  return get_ctx(id)->object;
}

uint32_t kernel_object_new(kernel_object_type t, void *obj) {
  SMART_CRITICAL_REGION
  uint32_t result;
  const pid_t begins = atomic_load(&id_seq);
  struct id_ctx find;
  do {
    result = atomic_add(&id_seq, 1);
    if (result == begins) {
      panic("running out of id");
    }
    if (result == 0) {
      continue;
    }
    find.id = result;
  } while (avl_tree_find(&id_tree, &find));
  struct id_ctx *ctx = malloc(sizeof(struct id_ctx));
  avl_node_init(&ctx->head);
  ctx->id = result;
  ctx->object = obj;
  ctx->type = t;
  avl_tree_add(&id_tree, ctx);
#ifdef VERBOSE
  terminal_fgcolor(CGA_COLOR_CYAN);
  printf("kernel_object_new: create kernel object %lld\n", (int64_t)result);
  terminal_default_color();
#endif
  // 当前的进程引用这个内核对象
  bool abs = kernel_object_ref(task_current(), result);
  if (!abs)
    abort();
  return result;
}

void kernel_object_delete(uint32_t id) {
  SMART_CRITICAL_REGION
  struct id_ctx *ctx = get_ctx(id);
  assert(ctx);
  void (*dtor)(void *) = get_dtor(ctx->type);
  assert(dtor);
  avl_tree_remove(&id_tree, ctx);
  dtor(ctx->object);
  free(ctx);
#ifdef VERBOSE
  terminal_fgcolor(CGA_COLOR_CYAN);
  printf("kernel_object_delete: delete kernel object %lld\n", (int64_t)id);
  terminal_default_color();
#endif
}

bool kernel_object_ref(ktask_t *task, uint32_t kobj_id) {
  SMART_CRITICAL_REGION
  struct id_ctx *ctx = get_ctx(kobj_id);
  if (!ctx)
    return false;
  // task这边记录对象id
  struct kern_obj_id *id_record = malloc(sizeof(struct kern_obj_id));
  assert(id_record);
  avl_node_init(&id_record->head);
  id_record->id = kobj_id;
  // 因为创建此线程的线程可能将此线程引用了该资源，所以要处理这种可能性
  if (avl_tree_add(&task->kernel_objects, id_record) == 0) {
    // 加引用计数
    uint32_t *counter = get_counter(ctx->type, ctx->object);
    (*counter)++;
#ifdef VERBOSE
    terminal_fgcolor(CGA_COLOR_CYAN);
    printf("kernel_object_ref: kernel object %lld ref to %lld\n",
           (int64_t)kobj_id, (int64_t)*counter);
    terminal_default_color();
#endif
  } else {
    free(id_record);
  }
  return true;
}

// 因为引用的时候需要关调度，所以封装一个“安全”版本出来
bool kernel_object_ref_safe(pid_t pid, uint32_t kobj_id) {
  SMART_CRITICAL_REGION
  ktask_t *task = task_find(pid);
  if (!task)
    return false;
  return kernel_object_ref(task, kobj_id);
}

void kernel_object_unref(ktask_t *task, uint32_t kobj_id,
                         bool remove_from_task_avl) {
  SMART_CRITICAL_REGION
  // 减引用计数
  struct id_ctx *ctx = get_ctx(kobj_id);
  uint32_t *counter = get_counter(ctx->type, ctx->object);
  (*counter)--;
  // task这边移除对象id
  struct kern_obj_id find;
  find.id = kobj_id;
  struct kern_obj_id *record = avl_tree_find(&task->kernel_objects, &find);
  if (remove_from_task_avl) {
    avl_tree_remove(&task->kernel_objects, record);
    free(record);
  }
// 如果引用归0，删除对象
#ifdef VERBOSE
  terminal_fgcolor(CGA_COLOR_CYAN);
  printf("kernel_object_unref: kernel object %lld unref to %lld\n",
         (int64_t)kobj_id, (int64_t)*counter);
  terminal_default_color();
#endif
  if (*counter == 0) {
    kernel_object_delete(kobj_id);
  }
}
