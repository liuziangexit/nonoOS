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
分配的是normal region内存，这些内存直接映射到内核空间
用buddy算法实现
*/
void kmem_page_debug();
void kmem_page_init();
void kmem_page_add_free_region(uintptr_t addr, uint32_t len);
void *kmem_page_alloc(size_t cnt);
void kmem_page_free(void *, size_t cnt);
void kmem_page_dump(void *dst, uint32_t dst_len);
bool kmem_page_compare_dump(void *a, void *b);
void kmem_page_print_dump(void *dmp);
enum MEMORY_REGION { NORMAL_REGION, FREE_REGION };
void *alloc_page_impl(enum MEMORY_REGION r, size_t cnt);
void free_page_impl(enum MEMORY_REGION r, uintptr_t p, size_t cnt);

/*
按对象分配内存的接口
用slab实现
https://www.kernel.org/doc/gorman/html/understand/understand011.html
*/
void kmem_cache_init();
void *kmem_cache_alloc(size_t alignment, size_t size);
bool kmem_cache_free(void *);

/*
按页分配内存的接口
分配的是free region内存，这些返回的物理页需要映射到虚拟空间后才能访问
*/
void free_region_init(struct e820map_t *memlayout);
uintptr_t free_region_page_alloc(size_t cnt);
void free_region_page_free(uintptr_t, size_t cnt);
// 将free region的某部分map到内核空间的map部分以供访问
// 返回值是指向physical的内核虚拟地址
void *free_region_access(uintptr_t physical, size_t length);
void free_region_no_access(void *virtual, size_t length);

#endif