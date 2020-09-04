#include <kmem_page.h>



void kmem_page_init(struct e820map_t *memlayout) {
    
}
void *kmem_page_alloc(size_t cnt);
void kmem_page_free(void *);
void kmem_page_dump();
