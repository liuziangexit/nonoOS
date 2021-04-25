#include <assert.h>
#include <memory_manager.h>
#include <stdlib.h>

#ifndef LIBNO_USER

void *malloc(size_t size) { return kmem_alloc(0, size); }

void *aligned_alloc(size_t alignment, size_t size) {
  assert(alignment == 0 || size % alignment == 0);
  return kmem_alloc(alignment, size);
}

void free(void *p) {
  bool ret = kmem_free(p);
  assert(ret);
}

#endif
