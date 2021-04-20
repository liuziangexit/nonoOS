#include <assert.h>
#include <memory_manager.h>
#include <stdlib.h>

void *malloc(size_t size) { abort(); }

void *aligned_alloc(size_t alignment, size_t size) {
  assert(size % alignment == 0);
  abort();
}

void free(void *p) { abort(); }
