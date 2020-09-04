#ifndef __KERNEL_KMEM_PAGE_H__
#define __KERNEL_KMEM_PAGE_H__
#include <defs.h>
#include <memlayout.h>

/*
按页分配内存的接口
用buddy算法实现
*/

void kmem_page_init(struct e820map_t *memlayout);
void *kmem_page_alloc(size_t cnt);
void kmem_page_free(void *);
void kmem_page_dump();

#endif