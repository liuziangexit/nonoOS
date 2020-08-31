#include <cga.h>
#include <defs.h>
#include <x86.h>

#define MONO_BASE 0x3B4
#define MONO_BUF 0xB0000
#define CGA_BASE 0x3D4
#define CGA_BUF 0xB8000

static uint16_t *crt_buf;
// http://cpctech.cpc-live.com/docs/mc6845/mc6845.htm
static uint16_t addr_6845;

void cga_init() {
  volatile uint16_t *cp = (uint16_t *)CGA_BUF;
  // 测试是否可写
  uint16_t prev = *cp;
  *cp = (uint16_t)0xA55A;
  if (*cp != 0xA55A) {
    //若不可写，则表示输出设备是黑白的
    cp = (uint16_t *)MONO_BUF;
    addr_6845 = MONO_BASE;
  } else {
    //若可写，则表示输出设备是CGA兼容的
    *cp = prev;
    addr_6845 = CGA_BASE;
  }
  crt_buf = (uint16_t *)cp;
}

void cga_move_cursor(uint16_t pos) {
  outb(addr_6845, 14);
  outb(addr_6845 + 1, pos >> 8);
  outb(addr_6845, 15);
  outb(addr_6845 + 1, pos);
}

uint16_t cga_get_cursor() {
  uint16_t pos;
  outb(addr_6845, 14);
  pos = inb(addr_6845 + 1) << 8;
  outb(addr_6845, 15);
  pos |= inb(addr_6845 + 1);
  return pos;
}

void cga_hide_cursor() { cga_move_cursor(CRT_SIZE); }

static inline uint16_t cga_entry(char uc, enum cga_color fg,
                                 enum cga_color bg) {
  return (((uint16_t)((bg << 4) | fg)) << 8) | uc;
}

void cga_write(uint16_t pos, enum cga_color bg, enum cga_color fg,
               const char *src, uint16_t size) {
  for (const char *w = src; w != src + size; w++) {
    crt_buf[pos + (uint16_t)(intptr_t)(w - src)] = cga_entry(*w, fg, bg);
  }
}

uint16_t *cga_buf() { return crt_buf; }
