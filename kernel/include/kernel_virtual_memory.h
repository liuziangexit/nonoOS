/*
物理页分配是纯粹纸上谈兵，不需要实际访问那些页，只需要摆弄那些页的地址就行了
每个zone有一个有序int32数组，每个数组项表示一个页的地址，就像vector那样
最长的数组是1页的那个zone里的，系统中总共有1M个页，但是这个zone里最多只有一半，
也就是512k个，每页占4字节，一共就是2M的大小，需要512个4k页


分配一个新的slab到cache的时候，还是按以前的去做
从slab里分配对象的时候，直接把这一整个slab页映射到内核空间（如果没有），然后返回这次分配的对象的地址。
从slab里回收对象的时候，如果这整个slab都空了，就把他在内核的映射去掉

分配页对象的时候，就更简单了，去拿到物理页，然后map到内核。回收的时候unmap然后free
page




一个bare hashmap记录physical page -> virtual page
系统中最多有4G/4K=1M个页
每个页的entry，也就是key和value的组合占用4+4=8B
hashmap一共1M*8B=8MB大，也就是2048个4K页

当我们想要把任何给定的地址n映射到内核空间，我们：
var virtual_memory = bare_hashmap.get(ROUNDDOWN(n, 4K))
if(!virtual_memory) {
    virtual_memory = find_free_vma();
    bare_hashmap.put(ROUNDDOWN(n, 4K), virtual_memory);
}
return virtual_memory + (n - ROUNDDOWN(n, 4K));

到这里为止我们实现的都只是kmalloc，vmalloc还要继续研究新的算法来实现
*/