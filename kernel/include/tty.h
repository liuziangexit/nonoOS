#ifndef __KERNEL_TTY_H__
#define __KERNEL_TTY_H__

#include <assert.h>
#include <cga.h>
#include <defs.h>
#include <list.h>
#include <ring_buffer.h>

#define TER_IN_BUF_LEN 512
#define TER_OUT_BUF_LEN (CRT_SIZE * 8192)

struct terminal {
  list_entry_t head;
  uint32_t id;

  enum cga_color fg;
  enum cga_color bg;

  // 现在viewport显示的页面
  uint32_t viewport;
  // 输出buffer写入位置
  uint32_t ob_wpos;

  unsigned char _in_buf[TER_IN_BUF_LEN];
  struct ring_buffer input_buffer;

  // 输出缓冲区
  // 储存最近4页的输出内容，用来实现滚屏
  // 必须是2的倍数并且大于2页，不然cut那里会有bug
  unsigned char output_buffer[TER_OUT_BUF_LEN];
  unsigned char output_color[TER_OUT_BUF_LEN];
};
typedef struct terminal terminal_t;

// 确保kbd isr不会打断程序引发的terminal访问
void terminal_blockbd();
void terminal_unblockbd();
bool terminal_kbdblocked();

void terminal_module_init();
void terminal_struct_init(terminal_t *ter);
void terminal_putchar(char c);
void terminal_write(const char *data, size_t size);
void terminal_write_string(const char *s);
void terminal_color(enum cga_color _fg, enum cga_color _bg);
void terminal_fgcolor(enum cga_color _fg);
void terminal_default_color();
struct ring_buffer *terminal_input_buffer();
int terminal_read_line(char *dst, int len);

void terminal_viewport_up(uint32_t line);
void terminal_viewport_down(uint32_t line);
void terminal_viewport_top();
void terminal_viewport_bottom();
#endif