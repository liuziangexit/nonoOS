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
size_t kgets(char *str, size_t str_len) {
  ktask_t *current = task_current();

  // 关闭中断防止键盘isr触发
  // 原因见kbd.c注释
  disable_interrupt();
  SMART_LOCK(l, current->group->input_buffer_mutex);

  char *p;
CV_LOOP:
  uint32_t cnt = ring_buffer_readable(&current->group->input_buffer);
  uint32_t line_len = 0;

  while (true) {
    p = ring_buffer_foreach(&current->group->input_buffer, &line_len, cnt);
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

  // 已找到行

  if (str_len < line_len) {
    // 行长度大于缓冲区长度
    enable_interrupt();
    return line_len;
  }

  uint32_t actual_read =
      ring_buffer_read(&current->group->input_buffer, str, line_len);
  assert(actual_read == line_len);
  assert(*(str + line_len - 1) == '\n' || *(str + line_len - 1) == EOF);
  *(str + line_len - 1) = '\0';
  enable_interrupt();
  return actual_read;
}

int getchar() {
  ktask_t *current = task_current();

  // 关闭中断防止键盘isr触发
  // 原因见kbd.c注释
  disable_interrupt();
  SMART_LOCK(l, current->group->input_buffer_mutex);

CV_LOOP:
  uint32_t cnt = ring_buffer_readable(&current->group->input_buffer);

  if (cnt == 0) {
    // 一个都没有！
    // 等待kbd.c里面发信号
    enable_interrupt();
    condition_variable_wait(current->group->input_buffer_cv,
                            current->group->input_buffer_mutex, false);
    // 这里需要在重新拿锁之前关闭中断，原因同上
    disable_interrupt();
    mutex_lock(current->group->input_buffer_mutex);
    goto CV_LOOP;
  }

  // 有至少一个字符
  char ch;
  uint32_t actual_read =
      ring_buffer_read(&current->group->input_buffer, &ch, 1);
  assert(actual_read == 1);
  enable_interrupt();
  return ch;
}

#else

#include <syscall.h>

// 用户态
char *gets(char *str) { return (char *)syscall(SYSCALL_GETS, 1, str); }

int getchar() { return syscall(SYSCALL_GETCHAR, 0); }

#endif