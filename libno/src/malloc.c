#include "stdlib.h"
#include <memory_manager.h>

void *malloc(size_t size) { return kmem_cache_alloc(size); }

void free(void *p) { kmem_cache_free(p); }
