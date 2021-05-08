#ifndef __KERNEL_USER_MALLOC_H__
#define __KERNEL_USER_MALLOC_H__

/*
为了实现用户malloc，我们需要实现三个函数
1)malloc
这个函数首先会在PCB里找到一个已分配且有对应物理页的虚拟页，
对于每一页，使用first-fit寻找*可用的空间，如果找到就返回。
如果没有找到，在PCB里找到一个已分配但没有对应物理页的虚拟页，
然后对它使用malloc_pgfault分配物理页，然后回到首步

2)malloc_pgfault
这个函数在发生缺页，并且缺页地址的vma的type是malloc时，
被用来给该malloc的vma分配物理页。malloc类型的vma，
要么全部分配，要么全没分配。

3)free
...

*:使用类似于老版slab那样的方法，直接将内存页中每个空闲块头部视为一个list_entry，
并在PCB里有一个这链表的头部，这样我们就有一个可用空间的链表

*/

#endif
