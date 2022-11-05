#ifndef __KERNEL_TTY_H__
#define __KERNEL_TTY_H__

#include <assert.h>
#include <cga.h>
#include <defs.h>
#include <list.h>
#include <ring_buffer.h>

// 确保kbd isr不会打断程序引发的terminal访问
void terminal_block_kbd();
void terminal_unblock_kbd();
bool terminal_is_kbd_blocked();

// 输入缓冲区
#define TER_IN_BUF_LEN 512
// 输出缓冲区
// 储存最近4页的输出内容，用来实现滚屏
// 必须是2的倍数并且大于2页，不然cut那里会有bug
#define TER_OUT_BUF_LEN (CRT_SIZE * 8192)

// 表示每个终端
struct terminal {
  list_entry_t head;
  uint32_t id;

  enum cga_color fg;
  enum cga_color bg;

  // 现在viewport显示的页面
  uint32_t viewport;
  // 输出buffer写入位置
  uint32_t obuf_wpos;

  unsigned char _in_buf[TER_IN_BUF_LEN];
  struct ring_buffer input_buffer;

  // 输出缓冲区
  // 储存最近4页的输出内容，用来实现滚屏
  // 必须是2的倍数并且大于2页，不然cut那里会有bug
  unsigned char output_buffer[TER_OUT_BUF_LEN];
  unsigned char output_color[TER_OUT_BUF_LEN];
};
typedef struct terminal terminal_t;

// 以下为对外接口

// 终端管理
void terminal_public_init();                            // 模块初始化
void terminal_public_new(terminal_t *ter, bool active); // 创建新termianl对象
void terminal_public_active(uint32_t id);               // 激活某个终端
void terminal_public_active_left();  // 激活当前终端左边的终端
void terminal_public_active_right(); // 激活当前终端右边的终端
void terminal_public_putchar(char c);
void terminal_public_write(const char *data, size_t size);
void terminal_public_write_string(const char *s);

void terminal_public_viewport_up(uint32_t line);
void terminal_public_viewport_down(uint32_t line);
void terminal_public_viewport_top();
void terminal_public_viewport_bottom();

// 以下为内部操作，其他模块不应该使用
terminal_t *terminal_private_current();

// 每终端操作
void terminal_private_putchar(terminal_t *ter, char c);
void terminal_private_write(terminal_t *ter, const char *data, size_t size);
void terminal_private_write_string(terminal_t *ter, const char *s);
void terminal_private_color(terminal_t *ter, enum cga_color _fg,
                            enum cga_color _bg);
void terminal_private_fgcolor(terminal_t *ter, enum cga_color _fg);
void terminal_private_default_color(terminal_t *ter);
struct ring_buffer *terminal_private_input_buffer(terminal_t *ter);
int terminal_private_read_line(terminal_t *ter, char *dst, int len);
#endif
