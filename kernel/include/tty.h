#ifndef __KERNEL_TTY_H
#define __KERNEL_TTY_H

#include "vga_color.h"
#include <stddef.h>

void terminal_initialize(enum vga_color fg, enum vga_color bg);
void terminal_putchar(char c);
void terminal_write(const char *data, size_t size);
void terminal_writestring(const char *data);

#endif
