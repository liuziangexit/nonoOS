#include <ctl_char_handler.h>
#include <kbd.h>
#include <stdio.h>
#include <tty.h>

void control_character_handler(int32_t *c, uint32_t shift) {
  if (*c == 8) {
    if (shift == (CTL | ALT)) {
      // outb(0x92, 0x3);
    } else {
      printf("\nDEL\n");
      *c = EOF;
      return;
    }
  }

  if (shift == CTL && *c == 'c') {
    printf("\nCTL+C\n");
    *c = EOF;
    return;
  }

  if (shift == CTL && *c == 'z') {
    printf("\nCTL+Z\n");
    *c = EOF;
    return;
  }

  // 控制屏幕滚动
  switch (*c) {
  case KEY_UP:
    *c = EOF;
    if (shift == CTL) {
      terminal_viewport_up(CRT_ROWS);
    } else {
      terminal_viewport_up(1);
    }
    break;
  case KEY_DN:
    *c = EOF;
    if (shift == CTL) {
      terminal_viewport_down(CRT_ROWS);
    } else {
      terminal_viewport_down(1);
    }
    break;
  case KEY_LF:
  case KEY_HOME:
    *c = EOF;
    terminal_viewport_top();
    break;
  case KEY_RT:
  case KEY_END:
    *c = EOF;
    terminal_viewport_bottom();
    break;
  case KEY_PGDN:
    *c = EOF;
    terminal_viewport_down(CRT_ROWS);
    break;
  case KEY_PGUP:
    *c = EOF;
    terminal_viewport_up(CRT_ROWS);
    break;
  }
}
