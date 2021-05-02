#include <assert.h>

#ifndef LIBNO_USER

#include <memory_manager.h>
#include <stdlib.h>

void *malloc(size_t size) { return kmem_alloc(0, size); }

void *aligned_alloc(size_t alignment, size_t size) {
  assert(alignment == 0 || size % alignment == 0);
  return kmem_alloc(alignment, size);
}

void free(void *p) {
  bool ret = kmem_free(p);
  assert(ret);
}

#else

//用户态实现
#include <syscall.h>
void *malloc(size_t size) { return (void *)syscall(SYSCALL_ALLOC, 2, 0, size); }
void *aligned_alloc(size_t alignment, size_t size) {
  assert(alignment == 0 || size % alignment == 0);
  return (void *)syscall(SYSCALL_ALLOC, 2, alignment, size);
}
void free(void *p) { syscall(SYSCALL_FREE, 1, p); }

#endif
