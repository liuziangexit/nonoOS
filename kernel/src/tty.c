#include "cga.h"
#include <defs.h>
#include <string.h>
#include <tty.h>

static inline uint8_t vga_entry_color(enum cga_color fg, enum cga_color bg) {
  return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
  return (uint16_t)uc | (uint16_t)color << 8;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t *const VGA_DIRECT_MEMORY = (uint16_t *)0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t *terminal_buffer;

void terminal_initialize(enum cga_color fg, enum cga_color bg) {
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = vga_entry_color(fg, bg);
  terminal_buffer = VGA_DIRECT_MEMORY;
  for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
    terminal_buffer[i] = vga_entry(' ', terminal_color);
  }
}

void terminal_setcolor(uint8_t color) { terminal_color = color; }

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
  unsigned char uc = c;
  if (uc == '\n') {
    terminal_column = 0;
    terminal_row++;
    terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
    return;
  }
  terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
  if (++terminal_column == VGA_WIDTH) {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT)
      terminal_row = 0;
  }
}

void terminal_write(const char *data, size_t size) {
  for (size_t i = 0; i < size; i++)
    terminal_putchar(data[i]);
}

void terminal_writestring(const char *data) {
  terminal_write(data, strlen(data));
}
