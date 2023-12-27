#include <assert.h>
#include <compiler_helper.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

#ifndef LIBNO_USER
#include <tty.h>

int vprintf(const char *format, va_list va);

int putchar(int ic) {
  char c = (char)ic;
  terminal_putchar(c);
  return ic;
}

int printf_color(enum cga_color color, const char *restrict format, ...) {
  terminal_fgcolor(color);
  va_list parameters;
  va_start(parameters, format);
  int ret = vprintf(format, parameters);
  va_end(parameters);
  terminal_default_color();
  return ret;
}

int puts(const char *string) { return printf("%s\n", string); }

#include "kernel_printf_impl.c"

#else

#include <syscall.h>

int putchar(int ic) {
  printf("%c", ic);
  return ic;
}

int printf(const char *restrict format, ...) {
  va_list parameters;
  va_start(parameters, format);
  uint32_t ret = syscall(SYSCALL_PRINTF, 2, format, &parameters);
  va_end(parameters);
  return ret;
}

int puts(const char *string) { return printf("%s\n", string); }

#endif