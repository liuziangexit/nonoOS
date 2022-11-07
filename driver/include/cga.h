#ifndef __DRIVER_CGA_H__
#define __DRIVER_CGA_H__

#include <defs.h>

#define CRT_ROWS 25
#define CRT_COLS 80
#define CRT_SIZE (CRT_ROWS * CRT_COLS)

enum cga_color {
  CGA_COLOR_BLACK = 0,
  CGA_COLOR_BLUE = 1,
  CGA_COLOR_GREEN = 2,
  CGA_COLOR_CYAN = 3,
  CGA_COLOR_RED = 4,
  CGA_COLOR_MAGENTA = 5,
  CGA_COLOR_BROWN = 6,
  CGA_COLOR_LIGHT_GREY = 7,
  CGA_COLOR_DARK_GREY = 8,
  CGA_COLOR_LIGHT_BLUE = 9,
  CGA_COLOR_LIGHT_GREEN = 10,
  CGA_COLOR_LIGHT_CYAN = 11,
  CGA_COLOR_LIGHT_RED = 12,
  CGA_COLOR_LIGHT_MAGENTA = 13,
  CGA_COLOR_LIGHT_YELLOW = 14,
  CGA_COLOR_WHITE = 15
};
typedef enum cga_color cga_color_t;

void cga_enable_indirect_mem();
void cga_init();
void cga_move_cursor(uint16_t pos);
uint16_t cga_get_cursor();
void cga_hide_cursor();
void cga_write(uint16_t pos, enum cga_color bg, enum cga_color fg,
               const char *src, uint16_t size);

#endif
