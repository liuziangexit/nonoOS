#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <stdio.h>
#include <tty.h>
#include <vga_color.h>

// entry.S中使用的页目
// 页目和页表必须对齐到页边界
// PTE_PS in a page directory entry enables 4Mbyte pages.

_Alignas(PGSIZE) int32_t page_directory[PGDIR_ENTRIES] = {
    // Map Virtual's [0, 4MB) to Physical's [0, 4MB)
    [0] = 0x0 | PTE_P | PTE_W | PTE_PS,
    // Map Virtual's [KERNBASE, KERNBASE+4MB) to Physical's [0, 4MB)
    [KERNEL_VIRTUAL_BASE >> PDXSHIFT] = 0x0 | PTE_P | PTE_W | PTE_PS,
};

void init_terminal() {
  terminal_initialize(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
}

void init_paging() {}

void kentry(void) {
  init_terminal();
  printf("Welcome...\n");
  printf("Initializing nonoOS...\n");
  init_paging();
  printf("Paging enabled\n");

  //
  while (1)
    ;
}
