#include <compiler_helper.h>
#include <defs.h>
#include <stdio.h>

#ifndef LIBNO_USER

#include "../../kernel/include/task.h"
#include <kbd.h>
#include <ring_buffer.h>
#include <sync.h>

// 复制输入到str去
// https://www.ibm.com/docs/en/i/7.2?topic=functions-gets-read-line
char *gets(char *str) {
  size_t len = getslen();
  ktask_t *current = task_current();
  uint32_t actual_read =
      ring_buffer_read(&current->group->input_buffer, str, len);
  assert(actual_read == len);
  assert(*(str + len - 1) == '\n' || *(str + len - 1) == EOF);
  *(str + len - 1) = '\0';
  enable_interrupt();
  return str;
}

size_t getslen() {
  ktask_t *current = task_current();

  // 关闭中断防止键盘isr触发
  // 原因见kbd.c注释
  disable_interrupt();
  SMART_LOCK(l, current->group->input_buffer_mutex);

  char *p;
CV_LOOP:
  uint32_t cnt = ring_buffer_readable(&current->group->input_buffer);
  uint32_t it = 0;

  while (true) {
    p = ring_buffer_foreach(&current->group->input_buffer, &it, cnt);
    if (!p) {
      // 遍历完了
      break;
    }
    if (*p == '\n' || *p == EOF) {
      break;
    }
  }

  assert(!p || (*p == '\n' || *p == EOF));

  if (!p) {
    // 遍历完了却没有遇到\n或EOF
    // 等待kbd.c里面发信号
    enable_interrupt();
    condition_variable_wait(current->group->input_buffer_cv,
                            current->group->input_buffer_mutex, false);
    // 这里需要在重新拿锁之前关闭中断，原因同上
    disable_interrupt();
    mutex_lock(current->group->input_buffer_mutex);
    goto CV_LOOP;
  }

  return it;
}

int getchar() {
  return EOF;
  // char c;
  // if (1 != ring_buffer_read(terminal_input_buffer(), &c, 1))
  //   return EOF;
  // return c;
}

#else

// TODO implement
char *gets(char *str) {
  UNUSED(str);
  return NULL;
}

int getchar() { return EOF; }

#endif