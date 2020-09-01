#ifndef __KERNEL_E820MAP_H__
#define __KERNEL_E820MAP_H__
#include <memlayout.h>
#include <stdbool.h>
#include <stdio.h>
#include <tty.h>

struct e820map_t *e820map = (struct e820map_t *)P2V(0x8000);

void print_e820() {
  printf("e820map:\n");
  printf("****************\n");
  int i;
  uint32_t total = 0, aval = 0;
  for (i = 0; i < e820map->count; i++) {
    uint64_t begin = e820map->ard[i].addr,
             end = e820map->ard[i].addr + e820map->ard[i].size;
    printf("[0x%08x, 0x%08x), size = 0x%08x, type = ", (uint32_t)begin,
           (uint32_t)end, (uint32_t)e820map->ard[i].size);
    bool is_available = E820_ADDR_AVAILABLE(e820map->ard[i].type);
    if (is_available) {
      terminal_fgcolor(CGA_COLOR_LIGHT_GREEN);
      aval += e820map->ard[i].size;
    } else {
      terminal_fgcolor(CGA_COLOR_RED);
    }
    total += e820map->ard[i].size;
    printf("%s\n", is_available ? "available" : "reserved");
    terminal_default_color();
  }
  printf("****************\n");
  printf("total: %dMB, available: %dMB\n", total / 1024 / 1024,
         aval / 1024 / 1024);
}

#endif
