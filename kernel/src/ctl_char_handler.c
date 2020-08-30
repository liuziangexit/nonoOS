#include <cga.h>
#include <ctl_char_handler.h>
#include <kbd.h>

void control_character_handler(int32_t *c, uint32_t *shift) {
  //方向键
  switch (*c) {
    uint16_t pos;
  case KEY_UP:
    *c = -1;
    pos = cga_get_cursor();
    if (pos >= CRT_COLS) {
      cga_move_cursor(cga_get_cursor() - CRT_COLS);
    } else {
      cga_move_cursor(0);
    }
    break;
  case KEY_DN:
    *c = -1;
    pos = cga_get_cursor();
    if (pos + CRT_COLS < CRT_SIZE) {
      cga_move_cursor(pos + CRT_COLS);
    } else {
      cga_move_cursor(CRT_SIZE - 1);
    }
    break;
  case KEY_LF:
    *c = -1;
    pos = cga_get_cursor();
    if (pos != 0) {
      cga_move_cursor(pos - 1);
    }
    break;
  case KEY_RT:
    *c = -1;
    pos = cga_get_cursor();
    if (pos != (CRT_SIZE - 1)) {
      cga_move_cursor(pos + 1);
    }
    break;
  }

  // Process special keys
  // Ctrl-Alt-Del: reboot
  if (!(~*shift & (CTL | ALT)) && *c == KEY_DEL) {
    outb(0x92, 0x3); // courtesy of Chris Frost
  }
}