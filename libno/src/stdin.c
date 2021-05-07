#include <compiler_helper.h>
#include <stdio.h>

#ifndef LIBNO_USER

#include <kbd.h>
#include <ring_buffer.h>
#include <tty.h>

char *gets(char *str) {
  if (0 == terminal_read_line(str, 512))
    return str;
  return NULL;
}

int getchar() {
  char c;
  if (1 != ring_buffer_read(terminal_input_buffer(), &c, 1))
    return EOF;
  return c;
}

#else

// TODO implement
char *gets(char *str) {
  UNUSED(str);
  return NULL;
}

int getchar() { return EOF; }

#endif