#ifndef __KERNEL_MEMEORY_MANAGER_H__
#define __KERNEL_MEMEORY_MANAGER_H__
#include <defs.h>
#include <memlayout.h>

void kmem_init(struct e820map_t *memlayout);

//跟std::max_align_t意思是一样的
#define MAX_ALIGNMENT 32

/*
分配内存的接口
*/
void kmem_alloc_init();
void *kmem_alloc(size_t alignment, size_t size);
bool kmem_free(void *);

/*
按页分配内存的接口
用buddy算法实现
*/
void kmem_page_init();
void *kmem_page_alloc(size_t cnt);
void kmem_page_free(void *, size_t cnt);
void kmem_page_dump(void *dst, uint32_t dst_len);
bool kmem_page_compare_dump(void *a, void *b);
void kmem_page_print_dump(void *dmp);

/*
按对象分配内存的接口
用slab实现
https://www.kernel.org/doc/gorman/html/understand/understand011.html
*/
void kmem_cache_init();
void *kmem_cache_alloc(size_t alignment, size_t size);
bool kmem_cache_free(void *);

#endif