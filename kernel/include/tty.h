#ifndef __KERNEL_TTY_H__
#define __KERNEL_TTY_H__

#include <cga.h>
#include <defs.h>

void terminal_init();
void terminal_putchar(char c);
void terminal_write(const char *data, size_t size);
void terminal_write_string(const char *s);
void terminal_color(enum cga_color _fg, enum cga_color _bg);
void terminal_fgcolor(enum cga_color _fg);
void terminal_default_color();
struct ring_buffer *terminal_input_buffer();
int terminal_read_line(char *dst, int len);
#endif
