#include <assert.h>
#include <cga.h>
#include <defs.h>
#include <panic.h>
#include <ring_buffer.h>
#include <string.h>
#include <tty.h>

static enum cga_color fg;
static enum cga_color bg;

// 现在viewport显示的页面
static uint16_t viewport_page = 0;

//输入缓冲区
#define TER_IN_BUF_LEN 512
static unsigned char _in_buf[TER_IN_BUF_LEN];
struct ring_buffer input_buffer;

//输出缓冲区
//储存最近4页的输出内容，用来实现滚屏
#define TER_OUT_BUF_LEN (CRT_SIZE * 4)
static unsigned char _out_buf[TER_OUT_BUF_LEN];
struct ring_buffer output_buffer;

static const char whitespace = ' ';

//清屏
static void viewport_clear() {
  for (uint16_t i = 0; i < CRT_SIZE; i++) {
    cga_write(i, bg, fg, &whitespace, 1);
  }
}

//看看一个位置在不在viewport里
static bool in_viewport(uint16_t pos) {
  if (pos < viewport_page)
    return false;
  if (pos == viewport_page)
    return true;
  if (pos > viewport_page)
    return pos < viewport_page + CRT_SIZE;
  __builtin_unreachable();
}

//更新光标位置到写位置，如果写位置不在viewport里，则不显示光标
static void viewport_update_cursor() {
  uint32_t write_pos = ring_buffer_readable(&output_buffer);
  if (in_viewport(write_pos)) {
    cga_move_cursor(write_pos - viewport_page);
  } else {
    cga_hide_cursor();
  }
}

//刷新viewport
static void viewport_update() {
  assert(viewport_page % CRT_COLS == 0);
  uint32_t tail = ring_buffer_readable(&output_buffer);
  if (viewport_page >= tail) {
    viewport_clear();
    viewport_update_cursor();
    return;
  }

  uint32_t end = viewport_page + CRT_SIZE;
  if (tail < end)
    end = tail;
  uint32_t it = viewport_page;
  void *p;
  viewport_clear();
  uint16_t write_idx = 0;
  while ((p = ring_buffer_foreach(&output_buffer, &it, end))) {
    cga_write(write_idx++, bg, fg, (char *)p, 1);
  }
  viewport_update_cursor();
}

void terminal_init() {
  ring_buffer_init(&input_buffer, _in_buf, TER_IN_BUF_LEN);
  ring_buffer_init(&output_buffer, _out_buf, TER_OUT_BUF_LEN);
  cga_init();
  viewport_update_cursor();
  terminal_default_color();
  viewport_clear();
}

void terminal_putchar(char c) {
  uint32_t write_pos = ring_buffer_readable(&output_buffer);
  if (c == '\n') {
    //写进buffer
    for (uint32_t i = 0; i < CRT_COLS - write_pos % CRT_COLS; i++) {
      ring_buffer_write(&output_buffer, true, &whitespace, 1);
    }
    //显示出来
    if (in_viewport(write_pos + CRT_COLS)) {
      //如果下一行在viewport中，不需要重绘整个viewport
      //更新光标位置
      viewport_update_cursor();
    } else {
      //如果下一行不在viewport中，则需要1.将viewport移动到最底下+1行2.重绘整个viewport
      // TODO 抽出来draw
      terminal_viewport_bottom();
      terminal_viewport_down();
    }
  } else {
    //写进buffer
    ring_buffer_write(&output_buffer, true, &c, 1);
    //显示出来
    if (in_viewport(write_pos)) {
      //如果写位置在viewport中，不需要重绘整个viewport
      cga_write((write_pos - viewport_page) % CRT_SIZE, bg, fg, &c, 1);
      //更新光标位置
      viewport_update_cursor();
    } else {
      //如果写位置不在viewport中，则需要1.将viewport移动到最底下+1行2.重绘整个viewport
      terminal_viewport_bottom();
      terminal_viewport_down();
    }
  }
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

// viewport向上移动一行
void terminal_viewport_up() {
  uint32_t written = ring_buffer_readable(&output_buffer);
  if (written < CRT_SIZE || viewport_page == 0) {
    return;
  }
  viewport_page -= CRT_COLS;
  viewport_update();
}

// viewport向下移动一行
void terminal_viewport_down() {
  uint32_t written = ring_buffer_readable(&output_buffer);
  if (written < CRT_SIZE) {
    return;
  }
  viewport_page += CRT_COLS;
  viewport_update();
}

// viewport移动到最顶部
void terminal_viewport_top() {
  uint32_t written = ring_buffer_readable(&output_buffer);
  if (written < CRT_SIZE) {
    return;
  }
  viewport_page = 0;
  viewport_update();
}

// viewport移动到最底部
void terminal_viewport_bottom() {
  uint32_t written = ring_buffer_readable(&output_buffer);
  if (written < CRT_SIZE) {
    return;
  }
  viewport_page = written - CRT_SIZE;
  viewport_page = viewport_page - viewport_page % CRT_COLS;
  viewport_page += CRT_COLS;
  viewport_update();
}
