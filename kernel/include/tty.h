#ifndef __KERNEL_TTY_H
#define __KERNEL_TTY_H

#include <cga.h>
#include <defs.h>

void terminal_initialize(enum cga_color fg, enum cga_color bg);
void terminal_putchar(char c);
void terminal_write(const char *data, size_t size);
void terminal_writestring(const char *data);

#endif
