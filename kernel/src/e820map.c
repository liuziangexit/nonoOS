#ifndef __KERNEL_E820MAP_H__
#define __KERNEL_E820MAP_H__
#include <memlayout.h>
#include <stdbool.h>
#include <stdio.h>
#include <tty.h>

struct e820map_t *e820map = (struct e820map_t *)P2V(0x8000);

void print_e820() {
  printf("****************\n");
  printf("e820map:\n");
  int i;
  for (i = 0; i < e820map->count; i++) {
    uint64_t begin = e820map->ard[i].addr, end = begin + e820map->ard[i].size;
    printf("[0x%08x, 0x%08x), size = 0x%08x, type = ", begin, end,
           e820map->ard[i].size);
    bool is_available = E820_ADDR_AVAILABLE(e820map->ard[i].type);
    if (is_available) {
      terminal_fgcolor(CGA_COLOR_LIGHT_GREEN);
    } else {
      terminal_fgcolor(CGA_COLOR_RED);
    }
    printf("%s\n", is_available ? "available" : "reserved");
    terminal_default_color();
  }
  printf("****************\n");
}

#endif
