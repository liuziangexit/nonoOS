#ifndef __KERNEL_TTY_H__
#define __KERNEL_TTY_H__

#include <assert.h>
#include <cga.h>
#include <defs.h>

// 确保kbd isr不会打断程序引发的terminal访问
// void terminal_blockbd();
// void terminal_unblockbd();
// bool terminal_kbdblocked();

void terminal_init();
void terminal_putchar(char c);
void terminal_write(const char *data, size_t size);
void terminal_write_string(const char *s);
void terminal_color(enum cga_color _fg, enum cga_color _bg);
void terminal_fgcolor(enum cga_color _fg);
void terminal_default_color();

void terminal_viewport_up(uint32_t line);
void terminal_viewport_down(uint32_t line);
void terminal_viewport_top();
void terminal_viewport_bottom();
#endif
