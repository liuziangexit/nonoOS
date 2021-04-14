#include "stdlib.h"
#include <memory_manager.h>

void *malloc(size_t size) { return kmem_cache_alloc(size); }

void *aligned_alloc(size_t alignment, size_t size) {
  assert(size % alignment == 0);
  return malloc(size);
}

void free(void *p) { kmem_cache_free(p); }
