#include <cga.h>
#include <defs.h>
#include <ring_buffer.h>
#include <string.h>
#include <tty.h>

static enum cga_color fg;
static enum cga_color bg;

static uint16_t write_idx = 0;

static void update_cursor() { cga_move_cursor(write_idx); }

//输入缓冲区
#define TER_IN_BUF_LEN 512
static unsigned char _in_buf[TER_IN_BUF_LEN];
struct ring_buffer input_buffer;

//输出缓冲区
#define TER_OUT_BUF_LEN (CRT_SIZE * 4)
static unsigned char _out_buf[TER_OUT_BUF_LEN];
struct ring_buffer output_buffer;

void terminal_init() {
  ring_buffer_init(&input_buffer, _in_buf, TER_IN_BUF_LEN);
  ring_buffer_init(&output_buffer, _out_buf, TER_OUT_BUF_LEN);
  cga_init();
  update_cursor();
  terminal_default_color();

  for (uint16_t i = 0; i < CRT_SIZE; i++) {
    terminal_putchar(' ');
  }
}

void terminal_putchar(char c) {
  if (c == '\n') {
    write_idx += CRT_COLS - write_idx % CRT_COLS;
  } else {
    cga_write(write_idx % CRT_SIZE, bg, fg, &c, 1);
    write_idx++;
  }
  write_idx %= CRT_SIZE;
  cga_move_cursor(write_idx);
}

void terminal_write(const char *data, size_t size) {
  for (uint32_t i = 0; i < size; i++) {
    terminal_putchar(data[i]);
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
