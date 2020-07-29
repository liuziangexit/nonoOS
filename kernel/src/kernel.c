#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <stdio.h>
#include <string.h>
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
  //临时小测试
  memset((void *)0x3FFC00, 0x8, 128);
  unsigned char look[128];
  memcpy((void *)look, (void *)(P2V(0x3FFC00)), 128);
  for (int32_t i = 0; i < 128; i++) {
    if (look[i] != 0x8) {
      while (1)
        ;
    }
  }

  init_terminal();
  printf("Welcome...\n");
  printf("Initializing nonoOS...\n");
  init_paging();
  printf("Paging enabled\n");

  //
  while (1)
    ;
}
