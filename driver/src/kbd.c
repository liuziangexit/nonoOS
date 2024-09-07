#include "task.h"
#include <assert.h>
#include <ctl_char_handler.h>
#include <interrupt.h>
#include <kbd.h>
#include <picirq.h>
#include <ring_buffer.h>
#include <shell.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sync.h>
#include <tty.h>

static uint8_t shiftcode[256] = {
    [0x1D] CTL, [0x2A] SHIFT, [0x36] SHIFT, [0x38] ALT, [0x9D] CTL, [0xB8] ALT};

static uint8_t togglecode[256] = {
    [0x3A] CAPSLOCK, [0x45] NUMLOCK, [0x46] SCROLLLOCK};

static uint8_t normalmap[256] = {NO,
                                 0x1B,
                                 '1',
                                 '2',
                                 '3',
                                 '4',
                                 '5',
                                 '6', // 0x00
                                 '7',
                                 '8',
                                 '9',
                                 '0',
                                 '-',
                                 '=',
                                 '\b',
                                 '\t',
                                 'q',
                                 'w',
                                 'e',
                                 'r',
                                 't',
                                 'y',
                                 'u',
                                 'i', // 0x10
                                 'o',
                                 'p',
                                 '[',
                                 ']',
                                 '\n',
                                 NO,
                                 'a',
                                 's',
                                 'd',
                                 'f',
                                 'g',
                                 'h',
                                 'j',
                                 'k',
                                 'l',
                                 ';', // 0x20
                                 '\'',
                                 '`',
                                 NO,
                                 '\\',
                                 'z',
                                 'x',
                                 'c',
                                 'v',
                                 'b',
                                 'n',
                                 'm',
                                 ',',
                                 '.',
                                 '/',
                                 NO,
                                 '*', // 0x30
                                 NO,
                                 ' ',
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 NO,
                                 '7', // 0x40
                                 '8',
                                 '9',
                                 '-',
                                 '4',
                                 '5',
                                 '6',
                                 '+',
                                 '1',
                                 '2',
                                 '3',
                                 '0',
                                 '.',
                                 NO,
                                 NO,
                                 NO,
                                 NO, // 0x50
                                 [0xC7] KEY_HOME,
                                 [0x9C] '\n' /*KP_Enter*/,
                                 [0xB5] '/' /*KP_Div*/,
                                 [0xC8] KEY_UP,
                                 [0xC9] KEY_PGUP,
                                 [0xCB] KEY_LF,
                                 [0xCD] KEY_RT,
                                 [0xCF] KEY_END,
                                 [0xD0] KEY_DN,
                                 [0xD1] KEY_PGDN,
                                 [0xD2] KEY_INS,
                                 [0xD3] KEY_DEL};

static uint8_t shiftmap[256] = {NO,
                                033,
                                '!',
                                '@',
                                '#',
                                '$',
                                '%',
                                '^', // 0x00
                                '&',
                                '*',
                                '(',
                                ')',
                                '_',
                                '+',
                                '\b',
                                '\t',
                                'q',
                                'w',
                                'e',
                                'r',
                                't',
                                'y',
                                'u',
                                'i', // 0x10
                                'o',
                                'p',
                                '{',
                                '}',
                                '\n',
                                NO,
                                'a',
                                's',
                                'd',
                                'f',
                                'g',
                                'h',
                                'j',
                                'k',
                                'l',
                                ':', // 0x20
                                '"',
                                '~',
                                NO,
                                '|',
                                'z',
                                'x',
                                'c',
                                'v',
                                'b',
                                'n',
                                'm',
                                '<',
                                '>',
                                '?',
                                NO,
                                '*', // 0x30
                                NO,
                                ' ',
                                NO,
                                NO,
                                NO,
                                NO,
                                NO,
                                NO,
                                NO,
                                NO,
                                NO,
                                NO,
                                NO,
                                NO,
                                NO,
                                '7', // 0x40
                                '8',
                                '9',
                                '-',
                                '4',
                                '5',
                                '6',
                                '+',
                                '1',
                                '2',
                                '3',
                                '0',
                                '.',
                                NO,
                                NO,
                                NO,
                                NO, // 0x50
                                [0xC7] KEY_HOME,
                                [0x9C] '\n' /*KP_Enter*/,
                                [0xB5] '/' /*KP_Div*/,
                                [0xC8] KEY_UP,
                                [0xC9] KEY_PGUP,
                                [0xCB] KEY_LF,
                                [0xCD] KEY_RT,
                                [0xCF] KEY_END,
                                [0xD0] KEY_DN,
                                [0xD1] KEY_PGDN,
                                [0xD2] KEY_INS,
                                [0xD3] KEY_DEL};

uint8_t shift_map(char ch) { return shiftmap[(int)ch]; }

// 大写变小写，小写变大写，全国变成🐱泽东思想大熔炉！
static char reverse_case(char c) {
  if ('a' <= c && c <= 'z')
    c += 'A' - 'a';
  else if ('A' <= c && c <= 'Z')
    c += 'a' - 'A';
  return c;
}

/* *
 * get data from keyboard
 * If we finish a character, return it, else 0. And return -1 if no data.
 * */
static int kbd_hw_read(void) {
  int32_t c;
  uint8_t data;
  static uint32_t shift = 0;

  if ((inb(KBSTATP) & KBS_DIB) == 0) {
    // 键盘input buffer是空的，没有数据了，退出！
    return EOF;
  }

  data = inb(KBDATAP);

  if (data == 0xE0) {
    // E0 escape character
    shift |= E0ESC;
    return EOF;
  } else if (data & 0x80) {
    // Key released
    data = (shift & E0ESC ? data : data & 0x7F);
    shift &= ~(shiftcode[data] | E0ESC);
    return EOF;
  } else if (shift & E0ESC) {
    // Last character was an E0 escape; or with 0x80
    data |= 0x80;
    shift &= ~E0ESC;
  }

  shift |= shiftcode[data];
  shift ^= togglecode[data];

  // printf("\nshift: %u\n", shift);

  if (shift & (CTL | SHIFT | ALT)) {
    // printf("\nSHIFT MAP\n");
    c = shiftmap[data];
  } else {
    // printf("\nNORMAL MAP\n");
    c = normalmap[data];
  }

  // 去除除了这三种以外的所有bits
  control_character_handler(&c, shift & (CTL | SHIFT | ALT));

  if (shift & CTL) {
    // printf("CTRL + ");
  }

  if (shift & SHIFT) {
    c = reverse_case(c);
    // printf("SHIFT + ");
  }

  if (shift & ALT) {
    // printf("ALT + ");
  }

  if (shift & CAPSLOCK) {
    c = reverse_case(c);
  }

  return c;
}

static uint32_t kbd_isr_lock;

void kbd_init(void) {
  //  drain the kbd buffer
  kbd_isr();
  // enable kbd interrupt
  pic_enable(IRQ_KBD);
  // printf("kbd ready!\n");
}

/* kbd_intr - try to feed input characters from keyboard */

// theres only one kbd_isr running at any given time

// while isr is running, there wont be any other process accessing terminal at
// that same time

// while one or more processes are accessing their terminals(whether its the
// displaying one or not), isr will not run
void kbd_isr() {
  SMART_CRITICAL_REGION //

  {
    // 已有一个kbd_isr正在运行
    // 退出当前kbd_isr
    uint32_t expected = 0;
    if (!atomic_compare_exchange(&kbd_isr_lock, &expected, 1,
                                 memory_order_acq_rel, memory_order_acq_rel)) {
      return;
    }
  }

  int c;
  while ((c = kbd_hw_read()) != EOF) {
    if (c != 0 && shell_ready()) {
      // echo
      terminal_write((char *)&c, 1);

      // 每输入一个字符，就放到task的inputbuf
      pid_t fg_pid = shell_fg();
      ktask_t *fg_task = task_find(fg_pid);
      task_group_t *fg_group = fg_task->group;

      // 当有读者正在访问input_buffer时，有可能键盘这边isr被中断触发，导致这里
      // 开始拿input_buffer_mutex锁尝试写入。这时候会导致死锁，因为锁正在被读者持有
      // 因此读者处需要关闭中断，防止此处键盘isr触发
      // 解决方案：在读取input_buffer时关中断
      // 请见stdin.c中的实现

      // 此时抢占是关闭状态，因为整个kbd isr是一个critical region
      // 这使得整个多任务系统退化成协作式
      // 但是这种条件下各个任务依然可以继续推进工作，因此不会造成死锁
      SMART_LOCK(l, fg_group->input_buffer_mutex);

      bool write_ok = ring_buffer_write(&fg_group->input_buffer, true, &c, 1);
      assert(write_ok);

      condition_variable_notify_one(fg_group->input_buffer_cv,
                                    fg_group->input_buffer_mutex);

      // 因为现在假如viewport不在最底下的时候就不刷viewport了，所以输入时候要手动刷viewport
      terminal_viewport_bottom();
    }
  }
  uint32_t prev = atomic_exchange(&kbd_isr_lock, 0);
  assert(prev == 1);
}
