#include <ctl_char_handler.h>
#include <kbd.h>
#include <tty.h>

void control_character_handler(int32_t *c, uint32_t *shift) {
  // 控制屏幕滚动
  switch (*c) {
  case KEY_UP:
    *c = -1;
    terminal_viewport_up(1);
    break;
  case KEY_DN:
    *c = -1;
    terminal_viewport_down(1);
    break;
  case KEY_LF:
    *c = -1;
    terminal_viewport_top();
    break;
  case KEY_RT:
    *c = -1;
    terminal_viewport_bottom();
    break;
  case KEY_PGDN:
    *c = -1;
    terminal_viewport_down(CRT_ROWS);
    break;
  case KEY_PGUP:
    *c = -1;
    terminal_viewport_up(CRT_ROWS);
    break;
  }

  // Process special keys
  // Ctrl-Alt-Del: reboot
  if (!(~*shift & (CTL | ALT)) && *c == KEY_DEL) {
    outb(0x92, 0x3); // courtesy of Chris Frost
  }
}