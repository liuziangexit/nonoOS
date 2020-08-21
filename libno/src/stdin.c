#include <kbd.h>
#include <ring_buffer.h>
#include <stdio.h>
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