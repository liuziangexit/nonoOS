#include <ctl_char_handler.h>
#include <kbd.h>
#include <shell.h>
#include <stdio.h>
#include <sync.h>
#include <task.h>
#include <tty.h>

void control_character_handler(int32_t *c, uint32_t shift) {
  if (*c == 8) {
    if (shift == (CTL | ALT)) {
      // outb(0x92, 0x3);
    } else {
      // 从task的inputbuf中的readable里移除
      pid_t fg_pid = shell_fg();
      ktask_t *fg_task = task_find(fg_pid);
      task_group_t *fg_group = fg_task->group;

      // 关于对inputbuffer的同步方式，参考kbd.c中的讲解
      SMART_LOCK(l, fg_group->input_buffer_mutex);
      uint32_t readable = ring_buffer_readable(&fg_group->input_buffer);
      if (readable >= 1) {
        terminal_backspace(1);
        bool ok = ring_buffer_unwrite(&fg_group->input_buffer, 1);
        assert(ok);
      } else {
        // 在terminal_backspace已经带有了这个
        // 但是在不能删除的情况下，我们就要额外做这个
        terminal_viewport_bottom();
      }
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
