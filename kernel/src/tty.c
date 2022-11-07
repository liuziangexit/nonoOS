#include <assert.h>
#include <cga.h>
#include <defs.h>
#include <panic.h>
#include <string.h>
#include <sync.h>
#include <tty.h>

terminal_t inital_terminal;

static const char whitespace = ' ';

static bool kbdblocked;
void terminal_blockbd() { kbdblocked = true; }
void terminal_unblockbd() { kbdblocked = false; }
bool terminal_kbdblocked() { return kbdblocked; }

#define MODULE_LOCK SMART_CRITICAL_REGION
#define TERMINAL_LOCK(t) SMART_CRITICAL_REGION

static terminal_t *get_task_binded_terminal() { return &inital_terminal; }

//清屏
static void viewport_clear() {
  SMART_CRITICAL_REGION
  for (uint32_t i = 0; i < CRT_SIZE; i++) {
    cga_write(i, get_task_binded_terminal()->bg, get_task_binded_terminal()->fg,
              &whitespace, 1);
  }
}

//看看一个位置在不在viewport里
static bool in_viewport(uint32_t pos) {
  if (pos < get_task_binded_terminal()->viewport)
    return false;
  if (pos == get_task_binded_terminal()->viewport)
    return true;
  if (pos > get_task_binded_terminal()->viewport)
    return pos < get_task_binded_terminal()->viewport + (uint32_t)CRT_SIZE;
  __builtin_unreachable();
}

// 更新光标位置到写位置，如果写位置不在viewport里，则不显示光标
static void viewport_update_cursor() {
  SMART_CRITICAL_REGION
  uint32_t write_pos = get_task_binded_terminal()->ob_wpos;
  if (in_viewport(write_pos)) {
    cga_move_cursor(write_pos - get_task_binded_terminal()->viewport);
  } else {
    cga_hide_cursor();
  }
}

// 刷新viewport
static void viewport_update() {
  assert(get_task_binded_terminal()->viewport % CRT_COLS == 0);
  uint32_t tail = get_task_binded_terminal()->ob_wpos;
  if (get_task_binded_terminal()->viewport >= tail) {
    viewport_clear();
    viewport_update_cursor();
    return;
  }

  uint32_t end = get_task_binded_terminal()->viewport + CRT_SIZE;
  if (tail < end)
    end = tail;
  viewport_clear();
  uint32_t write_idx = 0;
  for (uint32_t it = get_task_binded_terminal()->viewport; it < end;
       it++, write_idx++) {
    cga_write(write_idx, get_task_binded_terminal()->output_color[it] << 4,
              get_task_binded_terminal()->output_color[it],
              (const char *)(get_task_binded_terminal()->output_buffer + it),
              1);
  }
  viewport_update_cursor();
}

void terminal_module_init() {
  terminal_struct_init(&inital_terminal);
  viewport_update();
}

#define DEFAULT_FG CGA_COLOR_BLACK
#define DEFAULT_BG CGA_COLOR_LIGHT_GREY

void terminal_struct_init(terminal_t *ter) {
  memset(ter, 0, sizeof(terminal_t));
  list_init(&ter->head);
  ter->id = 1;
  ter->fg = DEFAULT_FG;
  ter->bg = DEFAULT_BG;
  ring_buffer_init(&ter->input_buffer, ter->_in_buf, TER_IN_BUF_LEN);
}

//把output buffer切两半，只留后面一半
static void ob_cut() {
  memcpy(get_task_binded_terminal()->output_buffer,
         get_task_binded_terminal()->output_buffer + TER_OUT_BUF_LEN / 2,
         TER_OUT_BUF_LEN / 2);
  memcpy(get_task_binded_terminal()->output_color,
         get_task_binded_terminal()->output_color + TER_OUT_BUF_LEN / 2,
         TER_OUT_BUF_LEN / 2);
  if (get_task_binded_terminal()->viewport > TER_OUT_BUF_LEN / 2) {
    get_task_binded_terminal()->viewport -= TER_OUT_BUF_LEN / 2;
  } else {
    get_task_binded_terminal()->viewport = 0;
  }
  get_task_binded_terminal()->ob_wpos = TER_OUT_BUF_LEN / 2;
}

void terminal_putchar(char c) {
  SMART_CRITICAL_REGION
  uint32_t write_pos = get_task_binded_terminal()->ob_wpos;
  if (c == '\n') {
    //写进buffer
    for (uint32_t i = 0; i < CRT_COLS - write_pos % CRT_COLS;
         i++, get_task_binded_terminal()->ob_wpos++) {
      get_task_binded_terminal()
          ->output_buffer[get_task_binded_terminal()->ob_wpos] = whitespace;
      get_task_binded_terminal()
          ->output_color[get_task_binded_terminal()->ob_wpos] =
          get_task_binded_terminal()->bg << 4 | get_task_binded_terminal()->fg;
    }
    //如果缓冲区满了，就切一半
    if (get_task_binded_terminal()->ob_wpos == TER_OUT_BUF_LEN) {
      ob_cut();
      write_pos = get_task_binded_terminal()->ob_wpos;
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
          get_task_binded_terminal()->viewport + (uint32_t)CRT_SIZE) {
        // 并且现在在viewport最底部
        // 则需要1.将viewport移动到最底下2.重绘整个viewport
        terminal_viewport_bottom();
      } else {
        //若不在最底部，则不要移动viewport
      }
    }
  } else {
    //写进buffer
    get_task_binded_terminal()
        ->output_buffer[get_task_binded_terminal()->ob_wpos] = c;
    get_task_binded_terminal()
        ->output_color[get_task_binded_terminal()->ob_wpos] =
        get_task_binded_terminal()->bg << 4 | get_task_binded_terminal()->fg;
    get_task_binded_terminal()->ob_wpos++;
    //如果缓冲区满了，就切一半
    if (get_task_binded_terminal()->ob_wpos == TER_OUT_BUF_LEN) {
      ob_cut();
      write_pos = get_task_binded_terminal()->ob_wpos;
      viewport_update();
      viewport_update_cursor();
    }
    //显示出来
    if (in_viewport(write_pos)) {
      //如果写位置在viewport中，不需要重绘整个viewport
      cga_write((write_pos - get_task_binded_terminal()->viewport) % CRT_SIZE,
                get_task_binded_terminal()->bg, get_task_binded_terminal()->fg,
                &c, 1);
      //更新光标位置
      viewport_update_cursor();
    } else {
      // 如果写位置不在viewport中
      if (write_pos ==
          get_task_binded_terminal()->viewport + (uint32_t)CRT_SIZE) {
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
  get_task_binded_terminal()->fg = _fg;
  get_task_binded_terminal()->bg = _bg;
}

void terminal_fgcolor(enum cga_color _fg) {
  get_task_binded_terminal()->fg = _fg;
}

void terminal_default_color() { terminal_color(DEFAULT_FG, DEFAULT_BG); }

struct ring_buffer *terminal_input_buffer() {
  return &get_task_binded_terminal()->input_buffer;
}

//返回0成功，返回-1失败表示没有找到行结尾，返回正数表示缓冲区过小，返回值是所需的缓冲区大小
//返回值末尾给了\0标记结束
int terminal_read_line(char *dst, int len) {
  SMART_CRITICAL_REGION
  //首先看看距离第一个\n有多远
  int max = ring_buffer_readable(&get_task_binded_terminal()->input_buffer);
  int n = 0;
  for (; n < max; n++) {
    if (*(dst + n) == '\n') {
      break;
    }
  }

  if (n == max) {
    return -1;
  }
  if (n > len) {
    return n;
  }

  ring_buffer_read(&get_task_binded_terminal()->input_buffer, dst, n - 1);
  dst[n] = 0;
  return 0;
}

// viewport向上移动一行
void terminal_viewport_up(uint32_t line) {
  SMART_CRITICAL_REGION
  uint32_t written = get_task_binded_terminal()->ob_wpos;
  if (written < CRT_SIZE || get_task_binded_terminal()->viewport == 0) {
    return;
  }
  if (get_task_binded_terminal()->viewport <= CRT_COLS * line) {
    get_task_binded_terminal()->viewport = 0;
  } else {
    get_task_binded_terminal()->viewport -= CRT_COLS * line;
  }
  viewport_update();
}

// viewport向下移动一行
void terminal_viewport_down(uint32_t line) {
  SMART_CRITICAL_REGION
  uint32_t written = get_task_binded_terminal()->ob_wpos;
  if (written < CRT_SIZE) {
    return;
  }
  uint32_t new_vp = get_task_binded_terminal()->viewport + CRT_COLS * line;
  if (new_vp > written) {
    get_task_binded_terminal()->viewport = ROUNDDOWN(written, CRT_COLS);
  } else {
    get_task_binded_terminal()->viewport = new_vp;
  }
  viewport_update();
}

// viewport移动到最顶部
void terminal_viewport_top() {
  SMART_CRITICAL_REGION
  uint32_t written = get_task_binded_terminal()->ob_wpos;
  if (written < CRT_SIZE) {
    return;
  }
  get_task_binded_terminal()->viewport = 0;
  viewport_update();
}

// viewport移动到最底部
void terminal_viewport_bottom() {
  SMART_CRITICAL_REGION
  uint32_t written = get_task_binded_terminal()->ob_wpos;
  if (written < CRT_SIZE) {
    return;
  }
  get_task_binded_terminal()->viewport = written - CRT_SIZE;
  get_task_binded_terminal()->viewport =
      get_task_binded_terminal()->viewport -
      get_task_binded_terminal()->viewport % CRT_COLS;
  get_task_binded_terminal()->viewport += CRT_COLS;
  viewport_update();
}