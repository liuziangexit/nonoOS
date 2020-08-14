#ifndef __DRIVER_CGA_H
#define __DRIVER_CGA_H

#include <defs.h>

enum cga_color {
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENTA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15
};

void cga_init();
void cga_move_cursor(uint16_t pos);
uint16_t cga_get_cursor();
void cga_write(uint16_t pos, enum cga_color bg, enum cga_color fg,
               unsigned char *src, uint16_t size);
uint16_t *cga_buf();

#endif