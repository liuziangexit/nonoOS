#include <memlayout.h>
#include <stdint.h>

struct virtual_memory kernel_vm;

uintptr_t normal_region_vaddr;
uint32_t normal_region_size;
uintptr_t normal_region_paddr;

uintptr_t map_region_vaddr;
uint32_t map_region_size;