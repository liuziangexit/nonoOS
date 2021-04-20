#ifndef __KERNEL_POW2_UTIL_H__
#define __KERNEL_POW2_UTIL_H__
#include <assert.h>
#include <defs.h>
#include <stdbool.h>
#include <stdint.h>

static inline __always_inline bool is_pow2(uint32_t x) {
  return !(x & (x - 1));
}

//输入2的n次幂，返回n
//如果输入不是2的幂次，行为未定义
static inline __always_inline uint32_t log2(uint32_t x) {
  assert(is_pow2(x));
  uint32_t idx;
  asm("bsrl %1, %0" : "=r"(idx) : "r"(x));
  return idx;
}

//返回2^exp
static inline __always_inline uint32_t pow2(uint32_t exp) { return 1 << exp; }

// 找到最近的大于等于x的2的幂(如果x已经是2的幂，返回x)
// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static inline __always_inline uint32_t next_pow2(uint32_t x) {
  assert(x > 0);
  if (is_pow2(x))
    return x;
  //下面这段代码啥意思呢，我们都知道2的幂的二进制表示是一个1后面跟着许多0的，
  //所以第一个大于x的2的幂应该是x的最高位左边那一位是1的一个数(比如1010的就是10000)
  //这一段代码做的是把x的位全部变成1，然后+1自然就是结果了
  //比如1010，首先把它变成1111，然后加1不就是10000了吗
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

//最小的有效输入是2
//如果x已经是2的幂，返回x
static inline __always_inline inline uint32_t prev_pow2(uint32_t x) {
  assert(x >= 2);
  if (is_pow2(x))
    return x;
  return next_pow2(x) / 2;
}

#endif