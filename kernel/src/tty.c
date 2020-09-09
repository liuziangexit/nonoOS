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
static uint16_t viewport = 0;

//输入缓冲区
#define TER_IN_BUF_LEN 512
static unsigned char _in_buf[TER_IN_BUF_LEN];
struct ring_buffer input_buffer;

//输出缓冲区
//储存最近4页的输出内容，用来实现滚屏
//必须是2的倍数并且大于2页，不然cut那里会有bug
#define TER_OUT_BUF_LEN (CRT_SIZE * 8)
static char output_buffer[TER_OUT_BUF_LEN];
static unsigned char output_color[TER_OUT_BUF_LEN];
static uint16_t ob_wpos = 0;

static const char whitespace = ' ';

//清屏
static void viewport_clear() {
  for (uint16_t i = 0; i < CRT_SIZE; i++) {
    cga_write(i, bg, fg, &whitespace, 1);
  }
}

//看看一个位置在不在viewport里
static bool in_viewport(uint16_t pos) {
  if (pos < viewport)
    return false;
  if (pos == viewport)
    return true;
  if (pos > viewport)
    return pos < viewport + CRT_SIZE;
  __builtin_unreachable();
}

//更新光标位置到写位置，如果写位置不在viewport里，则不显示光标
static void viewport_update_cursor() {
  uint32_t write_pos = ob_wpos;
  if (in_viewport(write_pos)) {
    cga_move_cursor(write_pos - viewport);
  } else {
    cga_hide_cursor();
  }
}

//刷新viewport
static void viewport_update() {
  assert(viewport % CRT_COLS == 0);
  uint32_t tail = ob_wpos;
  if (viewport >= tail) {
    viewport_clear();
    viewport_update_cursor();
    return;
  }

  uint32_t end = viewport + CRT_SIZE;
  if (tail < end)
    end = tail;
  viewport_clear();
  uint16_t write_idx = 0;
  for (uint16_t it = viewport; it < end; it++, write_idx++) {
    cga_write(write_idx, output_color[it] << 4, output_color[it],
              output_buffer + it, 1);
  }
  viewport_update_cursor();
}

void terminal_init() {
  ring_buffer_init(&input_buffer, _in_buf, TER_IN_BUF_LEN);
  cga_init();
  viewport_update_cursor();
  terminal_default_color();
  viewport_clear();
}

//把output buffer切两半，只留后面一半
static void ob_cut() {
  memcpy(output_buffer, output_buffer + TER_OUT_BUF_LEN / 2,
         TER_OUT_BUF_LEN / 2);
  memcpy(output_color, output_color + TER_OUT_BUF_LEN / 2, TER_OUT_BUF_LEN / 2);
  viewport = 0;
  ob_wpos = TER_OUT_BUF_LEN / 2;
}

void terminal_putchar(char c) {
  uint32_t write_pos = ob_wpos;
  if (c == '\n') {
    //写进buffer
    for (uint32_t i = 0; i < CRT_COLS - write_pos % CRT_COLS; i++, ob_wpos++) {
      output_buffer[ob_wpos] = whitespace;
      output_color[ob_wpos] = bg << 4 | fg;
    }
    //如果缓冲区满了，就切一半
    if (ob_wpos == TER_OUT_BUF_LEN) {
      ob_cut();
      write_pos = ob_wpos;
    }
    //显示出来
    if (in_viewport(write_pos + CRT_COLS)) {
      //如果下一行在viewport中，不需要重绘整个viewport
      //更新光标位置
      viewport_update_cursor();
    } else {
      //如果下一行不在viewport中，则需要1.将viewport移动到最底下2.重绘整个viewport
      terminal_viewport_bottom();
    }
  } else {
    //写进buffer
    output_buffer[ob_wpos] = c;
    output_color[ob_wpos] = bg << 4 | fg;
    ob_wpos++;
    //如果缓冲区满了，就切一半
    if (ob_wpos == TER_OUT_BUF_LEN) {
      ob_cut();
      write_pos = ob_wpos;
    }
    //显示出来
    if (in_viewport(write_pos)) {
      //如果写位置在viewport中，不需要重绘整个viewport
      cga_write((write_pos - viewport) % CRT_SIZE, bg, fg, &c, 1);
      //更新光标位置
      viewport_update_cursor();
    } else {
      //如果写位置不在viewport中，则需要1.将viewport移动到最底下2.重绘整个viewport
      terminal_viewport_bottom();
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
  uint32_t written = ob_wpos;
  if (written < CRT_SIZE || viewport == 0) {
    return;
  }
  viewport -= CRT_COLS;
  viewport_update();
}

// viewport向下移动一行
void terminal_viewport_down() {
  uint32_t written = ob_wpos;
  if (written < CRT_SIZE) {
    return;
  }
  uint32_t new_vp = viewport + CRT_COLS;
  if (new_vp > written)
    return;
  viewport = new_vp;
  viewport_update();
}

// viewport移动到最顶部
void terminal_viewport_top() {
  uint32_t written = ob_wpos;
  if (written < CRT_SIZE) {
    return;
  }
  viewport = 0;
  viewport_update();
}

// viewport移动到最底部
void terminal_viewport_bottom() {
  uint32_t written = ob_wpos;
  if (written < CRT_SIZE) {
    return;
  }
  viewport = written - CRT_SIZE;
  viewport = viewport - viewport % CRT_COLS;
  viewport += CRT_COLS;
  viewport_update();
}
