#include <assert.h>
#include <atomic.h>
#include <ctl_char_handler.h>
#include <interrupt.h>
#include <kbd.h>
#include <picirq.h>
#include <ring_buffer.h>
#include <stdio.h>
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
                                'Q',
                                'W',
                                'E',
                                'R',
                                'T',
                                'Y',
                                'U',
                                'I', // 0x10
                                'O',
                                'P',
                                '{',
                                '}',
                                '\n',
                                NO,
                                'A',
                                'S',
                                'D',
                                'F',
                                'G',
                                'H',
                                'J',
                                'K',
                                'L',
                                ':', // 0x20
                                '"',
                                '~',
                                NO,
                                '|',
                                'Z',
                                'X',
                                'C',
                                'V',
                                'B',
                                'N',
                                'M',
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

#define C(x) (x - '@')

static uint8_t ctlmap[256] = {NO,
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
                              NO,
                              NO,
                              C('Q'),
                              C('W'),
                              C('E'),
                              C('R'),
                              C('T'),
                              C('Y'),
                              C('U'),
                              C('I'),
                              C('O'),
                              C('P'),
                              NO,
                              NO,
                              '\r',
                              NO,
                              C('A'),
                              C('S'),
                              C('D'),
                              C('F'),
                              C('G'),
                              C('H'),
                              C('J'),
                              C('K'),
                              C('L'),
                              NO,
                              NO,
                              NO,
                              NO,
                              C('\\'),
                              C('Z'),
                              C('X'),
                              C('C'),
                              C('V'),
                              C('B'),
                              C('N'),
                              C('M'),
                              NO,
                              NO,
                              C('/'),
                              NO,
                              NO,
                              [0x97] KEY_HOME,
                              [0xB5] C('/'),
                              [0xC8] KEY_UP,
                              [0xC9] KEY_PGUP,
                              [0xCB] KEY_LF,
                              [0xCD] KEY_RT,
                              [0xCF] KEY_END,
                              [0xD0] KEY_DN,
                              [0xD1] KEY_PGDN,
                              [0xD2] KEY_INS,
                              [0xD3] KEY_DEL};

static uint8_t *charcode[4] = {normalmap, shiftmap, ctlmap, ctlmap};

/* *
 * get data from keyboard
 * If we finish a character, return it, else 0. And return -1 if no data.
 * */
static int kbd_hw_read(void) {
  int32_t c;
  uint8_t data;
  static uint32_t shift = 0;

  if ((inb(KBSTATP) & KBS_DIB) == 0) {
    return -1;
  }

  data = inb(KBDATAP);

  if (data == 0xE0) {
    // E0 escape character
    shift |= E0ESC;
    return 0;
  } else if (data & 0x80) {
    // Key released
    data = (shift & E0ESC ? data : data & 0x7F);
    shift &= ~(shiftcode[data] | E0ESC);
    return 0;
  } else if (shift & E0ESC) {
    // Last character was an E0 escape; or with 0x80
    data |= 0x80;
    shift &= ~E0ESC;
  }

  shift |= shiftcode[data];
  shift ^= togglecode[data];

  c = charcode[shift & (CTL | SHIFT)][data];
  if (shift & CAPSLOCK) {
    if ('a' <= c && c <= 'z')
      c += 'A' - 'a';
    else if ('A' <= c && c <= 'Z')
      c += 'a' - 'A';
  }

  control_character_handler(&c, &shift);

  return c;
}

void kbd_init(void) {
  // drain the kbd buffer
  kbd_isr();
  // enable kbd interrupt
  pic_enable(IRQ_KBD);
}

static uint32_t kbd_isr_lock;

/* kbd_intr - try to feed input characters from keyboard */

// theres only one kbd_isr running at any given time

// while isr is running, there wont be any other process accessing terminal at
// that same time

// while one or more processes are accessing their terminals(whether its the
// displaying one or not), isr will not run
void kbd_isr() {
  SMART_CRITICAL_REGION //
  {
    uint32_t expected = 0;
    if (!atomic_compare_exchange(&kbd_isr_lock, &expected, 1)) {
      return; // 另一个kbd_isr正在运行
    }
  }
  if (terminal_kbdblocked()) {
    // 有程序发起的terminal访问正在进行，本次中断引起的访问有可能与其冲突，忽略
    return;
  }
  int c;
  while ((c = kbd_hw_read()) != EOF) {
    if (c != 0) {
      // copy to input buffer
      ring_buffer_write(terminal_input_buffer(), false, &c, 1);
      // echo
      terminal_write((char *)&c, 1);
      // 因为现在假如viewport不在最底下的时候就不刷viewport了，所以输入时候要手动刷viewport
      terminal_viewport_bottom();
    }
  }
  uint32_t prev = atomic_exchange(&kbd_isr_lock, 0);
  assert(prev == 1);
}
