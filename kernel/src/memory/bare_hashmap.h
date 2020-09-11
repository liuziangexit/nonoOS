#ifndef __KERNEL_BARE_HASHMAP_H__
#define __KERNEL_BARE_HASHMAP_H__
#include <assert.h>
#include <defs.h>
#include <memory_manager.h>
#include <stdbool.h>
#include <string.h>

/*
这个类基本上是专门给kmem_alloc做的，为了记录里面分配的内存对应的slab指针
站在通用hashmap的角度看，这个类有以下特点

1.这个hashmap按页使用内存
2.key和value都是无符号32位整型
3.value不能是0
4.如果get一个不存在的key，开销很高

hashmap的核心在于，如何处理hash冲突。我们的处理方法是，存到下一个空位，如果下一位
依然已被使用，那么我们将一直线性搜索下去，直到绕了一圈发现没有空位了
这个显然不是一个很好的方法，但是鉴于我们现在还没有malloc，所以恐怕我们只能这样做了
*/

void bare_init(void *page, uint32_t pgcnt);
//可能引起grow，如果发生了，旧的page会被free，新的page位置和大小会通过npage和npgcnt返回
//如果没有发生grow，则npage和npgcnt会等于原来的值
//如果grow失败而因此put也失败了，npage会是0，此时npgcnt的值未定义
uint32_t bare_put(void *page, uint32_t pgcnt, uint32_t key, uint32_t value,
                  void **npage, uint32_t *npgcnt);
uint32_t bare_get(void *page, uint32_t pgcnt, uint32_t key);
uint32_t bare_del(void *page, uint32_t pgcnt, uint32_t key);
void bare_clear(void *page, uint32_t pgcnt);

#endif
