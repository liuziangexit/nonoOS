#include <ctl_char_handler.h>
#include <kbd.h>
#include <tty.h>

void control_character_handler(int32_t *c, uint32_t *shift) {
  //方向键
  switch (*c) {
    uint16_t pos;
  case KEY_UP:
    *c = -1;
    terminal_viewport_up();
    break;
  case KEY_DN:
    *c = -1;
    terminal_viewport_down();
    break;
  case KEY_LF:
    *c = -1;
    terminal_viewport_top();
    break;
  case KEY_RT:
    *c = -1;
    terminal_viewport_bottom();
    break;
  }

  // Process special keys
  // Ctrl-Alt-Del: reboot
  if (!(~*shift & (CTL | ALT)) && *c == KEY_DEL) {
    outb(0x92, 0x3); // courtesy of Chris Frost
  }
}