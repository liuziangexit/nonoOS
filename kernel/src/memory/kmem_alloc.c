#include <memory_manager.h>

// TODO
// 把kmem_cache里的那个hashmap拿到这里来，记录一个被分配出去内存位置的两种来源1)page
// 2)cache
//干脆就放两个hashmap
void *kmem_alloc(size_t alignment, size_t size) {}
void kmem_free(void *p) {}