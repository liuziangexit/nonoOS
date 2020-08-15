#ifndef __KERNEL_TTY_H
#define __KERNEL_TTY_H

#include <cga.h>
#include <defs.h>

void terminal_init(enum cga_color fg, enum cga_color bg);
void terminal_putchar(char c);
void terminal_write(const char *data, size_t size);
void terminal_write_string(const char *s);

#endif
