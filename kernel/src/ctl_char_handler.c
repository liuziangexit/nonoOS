#include <ctl_char_handler.h>
#include <kbd.h>

void control_character_handler(int32_t *c, uint32_t *shift) {
  //方向键
  switch (*c) {
  case KEY_UP:
    *c = 24;
    break;
  case KEY_DN:
    *c = 25;
    break;
  case KEY_LF:
    *c = 27;
    break;
  case KEY_RT:
    *c = 26;
    break;
  }

  // Process special keys
  // Ctrl-Alt-Del: reboot
  if (!(~*shift & (CTL | ALT)) && *c == KEY_DEL) {
    outb(0x92, 0x3); // courtesy of Chris Frost
  }
}