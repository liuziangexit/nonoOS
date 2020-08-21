#include <kbd.h>
#include <stdio.h>

int getchar() {
  char c;
  // FIXME busy wait
  while (!kbd_read(&c, 1))
    ;
  return c;
}
//char *gets(char *dst);
// int scanf(const char *__restrict, ...);
