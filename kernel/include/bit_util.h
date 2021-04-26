#ifndef __KERNEL_BIT_UTIL_H__
#define __KERNEL_BIT_UTIL_H__
#include <stdbool.h>
#include <stdint.h>

static inline uint32_t bit_set(uint32_t op, uint32_t idx, bool val) {
  if (val) {
    return op | (1 << idx);
  } else {
    return op & (~(1 << idx));
  }
}

static inline bool bit_test(uint32_t op, uint32_t idx) {
  return (op & (1 << idx)) != 0;
}

static inline uint32_t bit_scan_forward(uint32_t op) {
  uint32_t idx;
  asm("bsfl %1, %0" : "=r"(idx) : "r"(op));
  return idx;
}

static inline uint32_t bit_scan_reverse(uint32_t op) {
  uint32_t idx;
  asm("bsrl %1, %0" : "=r"(idx) : "r"(op));
  return idx;
}

static inline uint32_t bit_flip(uint32_t op) { return ~op; }

#endif