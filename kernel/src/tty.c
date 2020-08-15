#include <cga.h>
#include <defs.h>
#include <string.h>
#include <tty.h>

static const size_t CGA_WIDTH = 80;
static const size_t CGA_HEIGHT = 25;

static enum cga_color fg;
static enum cga_color bg;

static uint16_t write_col;
static uint16_t write_row;

static inline uint16_t index(uint16_t col, uint16_t row) {
  return col * CGA_WIDTH + row;
}

static void update_cursor() { cga_move_cursor(index(write_col, write_row)); }

void terminal_init(enum cga_color _fg, enum cga_color _bg) {
  cga_init();
  write_col = 0;
  write_row = 0;
  update_cursor();
  fg = _fg;
  bg = _bg;

  for (uint16_t i = 0; i < CGA_HEIGHT * CGA_WIDTH; i++) {
    cga_write(i, bg, fg, " ", 1);
  }
}

void terminal_putchar(char c) { terminal_write(&c, 1); }

static void nextline() {
  write_row = 0;
  if (++write_col == CGA_HEIGHT) {
    write_col = 0;
  }
  update_cursor();
}

void terminal_write(const char *data, size_t size) {
  uint16_t head = 0, tail = 0;
  for (; tail < size; tail++) {
    if (write_row + (tail - head) == CGA_WIDTH //
        || data[tail] == '\n') {
      cga_write(index(write_col, write_row), bg, fg, data + head, tail - head);
    }
    if (write_row + (tail - head) == CGA_WIDTH) {
      head = tail;
      nextline();
    }
    if (data[tail] == '\n') {
      head = tail + 1;
      nextline();
    }
  }
  if (head != tail) {
    cga_write(index(write_col, write_row), bg, fg, data + head, tail - head);
    write_row += tail - head;
    update_cursor();
  }
}

void terminal_write_string(const char *s) { terminal_write(s, strlen(s)); }
