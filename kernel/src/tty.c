#include <assert.h>
#include <cga.h>
#include <defs.h>
#include <panic.h>
#include <ring_buffer.h>
#include <string.h>
#include <sync.h>
#include <tty.h>

// terminal模块将屏幕整理为一个命令行的样子
// 但是它不懂得前台/后台程序，也不懂不同程序命令行之间的切换
// 这些逻辑实现在shell模块
// 从逻辑上说，我们的显示相关模块的栈是这样的，program>shell>tty>cga
// 这个显示栈负责引入gui之前的所有绘制

static enum cga_color fg;
static enum cga_color bg;

// 现在viewport显示的页面
static uint32_t viewport = 0;

//输出缓冲区
//储存最近4页的输出内容，用来实现滚屏
//必须是2的倍数并且大于2页，不然cut那里会有bug
#define TER_OUT_BUF_LEN (CRT_SIZE * 8192)
static char output_buffer[TER_OUT_BUF_LEN];
static unsigned char output_color[TER_OUT_BUF_LEN];
static uint32_t ob_wpos = 0;

static const char whitespace = ' ';

// static bool kbdblocked;
// void terminal_blockbd() { kbdblocked = true; }
// void terminal_unblockbd() { kbdblocked = false; }
// bool terminal_kbdblocked() { return kbdblocked; }

//清屏
static void viewport_clear() {
  SMART_CRITICAL_REGION
  for (uint32_t i = 0; i < CRT_SIZE; i++) {
    cga_write(i, bg, fg, &whitespace, 1);
  }
}

//看看一个位置在不在viewport里
static bool in_viewport(uint32_t pos) {
  if (pos < viewport)
    return false;
  if (pos == viewport)
    return true;
  if (pos > viewport)
    return pos < viewport + (uint32_t)CRT_SIZE;
  __builtin_unreachable();
}

//更新光标位置到写位置，如果写位置不在viewport里，则不显示光标
static void viewport_update_cursor() {
  SMART_CRITICAL_REGION
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
  uint32_t write_idx = 0;
  for (uint32_t it = viewport; it < end; it++, write_idx++) {
    cga_write(write_idx, output_color[it] << 4, output_color[it],
              output_buffer + it, 1);
  }
  viewport_update_cursor();
}

void terminal_init() {
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
  if (viewport > TER_OUT_BUF_LEN / 2) {
    viewport -= TER_OUT_BUF_LEN / 2;
  } else {
    viewport = 0;
  }
  ob_wpos = TER_OUT_BUF_LEN / 2;
}

void terminal_putchar(char c) {
  SMART_CRITICAL_REGION
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
      viewport_update();
      viewport_update_cursor();
    }
    //显示出来
    if (in_viewport(write_pos + CRT_COLS)) {
      //如果下一行在viewport中，不需要重绘整个viewport
      //更新光标位置
      viewport_update_cursor();
    } else {
      // 如果写位置不在viewport中
      if ((write_pos + CRT_COLS) - (write_pos % CRT_COLS) ==
          viewport + (uint32_t)CRT_SIZE) {
        // 并且现在在viewport最底部
        // 则需要1.将viewport移动到最底下2.重绘整个viewport
        terminal_viewport_bottom();
      } else {
        //若不在最底部，则不要移动viewport
      }
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
      viewport_update();
      viewport_update_cursor();
    }
    //显示出来
    if (in_viewport(write_pos)) {
      //如果写位置在viewport中，不需要重绘整个viewport
      cga_write((write_pos - viewport) % CRT_SIZE, bg, fg, &c, 1);
      //更新光标位置
      viewport_update_cursor();
    } else {
      // 如果写位置不在viewport中
      if (write_pos == viewport + (uint32_t)CRT_SIZE) {
        // 并且现在在viewport最底部
        // 则需要1.将viewport移动到最底下2.重绘整个viewport
        terminal_viewport_bottom();
      } else {
        //若不在最底部，则不要移动viewport
      }
    }
  }
}

void terminal_write(const char *data, size_t size) {
  SMART_CRITICAL_REGION
  for (uint32_t i = 0; i < size; i++) {
    terminal_putchar(data[i]);
  }
}

void terminal_write_string(const char *s) { terminal_write(s, strlen(s)); }

void terminal_color(enum cga_color _fg, enum cga_color _bg) {
  SMART_CRITICAL_REGION
  fg = _fg;
  bg = _bg;
}

void terminal_fgcolor(enum cga_color _fg) { fg = _fg; }

void terminal_default_color() {
  terminal_color(CGA_COLOR_BLACK, CGA_COLOR_LIGHT_GREY);
}

// viewport向上移动一行
void terminal_viewport_up(uint32_t line) {
  SMART_CRITICAL_REGION
  uint32_t written = ob_wpos;
  if (written < CRT_SIZE || viewport == 0) {
    return;
  }
  if (viewport <= CRT_COLS * line) {
    viewport = 0;
  } else {
    viewport -= CRT_COLS * line;
  }
  viewport_update();
}

// viewport向下移动一行
void terminal_viewport_down(uint32_t line) {
  SMART_CRITICAL_REGION
  uint32_t written = ob_wpos;
  if (written < CRT_SIZE) {
    return;
  }
  uint32_t new_vp = viewport + CRT_COLS * line;
  if (new_vp > written) {
    viewport = ROUNDDOWN(written, CRT_COLS);
  } else {
    viewport = new_vp;
  }
  viewport_update();
}

// viewport移动到最顶部
void terminal_viewport_top() {
  SMART_CRITICAL_REGION
  uint32_t written = ob_wpos;
  if (written < CRT_SIZE) {
    return;
  }
  viewport = 0;
  viewport_update();
}

// viewport移动到最底部
void terminal_viewport_bottom() {
  SMART_CRITICAL_REGION
  uint32_t written = ob_wpos;
  if (written < CRT_SIZE) {
    return;
  }
  viewport = written - CRT_SIZE;
  viewport = viewport - viewport % CRT_COLS;
  viewport += CRT_COLS;
  viewport_update();
}
