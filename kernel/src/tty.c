#include <cga.h>
#include <defs.h>
#include <ring_buffer.h>
#include <string.h>
#include <tty.h>

#define CGA_WIDTH 80
#define CGA_HEIGHT 25

static enum cga_color fg;
static enum cga_color bg;

static uint16_t write_col;
static uint16_t write_row;

static inline uint16_t index(uint16_t col, uint16_t row) {
  return col * CGA_WIDTH + row;
}

static void update_cursor() { cga_move_cursor(index(write_col, write_row)); }

/* *
 * Here we manage the console input buffer, where we stash characters
 * received from the keyboard or serial port whenever the corresponding
 * interrupt occurs.
 * */
#define TER_BUF_LEN 512
static unsigned char _buf[TER_BUF_LEN];
struct ring_buffer input_buffer;

void terminal_init() {
  ring_buffer_init(&input_buffer, _buf, TER_BUF_LEN);
  cga_init();
  write_col = 0;
  write_row = 0;
  update_cursor();
  terminal_default_color();

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

void terminal_color(enum cga_color _fg, enum cga_color _bg) {
  fg = _fg;
  bg = _bg;
}

void terminal_fgcolor(enum cga_color _fg) { fg = _fg; }

void terminal_default_color() {
  terminal_color(CGA_COLOR_BLACK, CGA_COLOR_LIGHT_GREY);
}

struct ring_buffer *terminal_input_buffer() {
  return &input_buffer;
}
//
//返回0成功，返回-1失败表示没有找到行结尾，返回正数表示缓冲区过小，返回值是所需的缓冲区大小
//返回值末尾给了\0标记结束
int terminal_read_line(char *dst, int len) {
  //首先看看距离第一个\n有多远
  int max = ring_buffer_readable(&input_buffer);
  int n = 0;
  for (; n < max; n++) {
    if (*(dst + n) == '\n') {
      break;
    }
  }

  if (n == max)
    return -1;
  if (n > len)
    return n;

  ring_buffer_read(&input_buffer, dst, n - 1);
  dst[n] = 0;
  return 0;
}