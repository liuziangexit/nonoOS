#include <math.h>
#include <stdint.h>

//想了一下，要实现一个正确的round还蛮复杂的，状态很多，testcase也难整，就不自己写了，直接抄一个来

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

//手动指出我们在小端平台(IA32)
#define __IEEE_LITTLE_ENDIAN 0

#ifdef __IEEE_BIG_ENDIAN
typedef union {
  double value;
  struct {
    uint32_t msw;
    uint32_t lsw;
  } parts;
} ieee_double_shape_type;
#endif

#ifdef __IEEE_LITTLE_ENDIAN
typedef union {
  double value;
  struct {
    uint32_t lsw;
    uint32_t msw;
  } parts;
} ieee_double_shape_type;
#endif

/* Get two 32 bit ints from a double.  */
#define EXTRACT_WORDS(ix0, ix1, d)                                             \
  do {                                                                         \
    ieee_double_shape_type ew_u;                                               \
    ew_u.value = (d);                                                          \
    (ix0) = ew_u.parts.msw;                                                    \
    (ix1) = ew_u.parts.lsw;                                                    \
  } while (0)

/* Set a double from two 32 bit ints.  */
#define INSERT_WORDS(d,ix0,ix1)					\
do {								\
  ieee_double_shape_type iw_u;					\
  iw_u.parts.msw = (ix0);					\
  iw_u.parts.lsw = (ix1);					\
  (d) = iw_u.value;						\
} while (0)

double round(double x) {
  /* Most significant word, least significant word. */
  int32_t msw, exponent_less_1023;
  uint32_t lsw;

  EXTRACT_WORDS(msw, lsw, x);

  /* Extract exponent field. */
  exponent_less_1023 = ((msw & 0x7ff00000) >> 20) - 1023;

  if (exponent_less_1023 < 20) {
    if (exponent_less_1023 < 0) {
      msw &= 0x80000000;
      if (exponent_less_1023 == -1)
        /* Result is +1.0 or -1.0. */
        msw |= ((int32_t)1023 << 20);
      lsw = 0;
    } else {
      uint32_t exponent_mask = 0x000fffff >> exponent_less_1023;
      if ((msw & exponent_mask) == 0 && lsw == 0)
        /* x in an integral value. */
        return x;

      msw += 0x00080000 >> exponent_less_1023;
      msw &= ~exponent_mask;
      lsw = 0;
    }
  } else if (exponent_less_1023 > 51) {
    if (exponent_less_1023 == 1024)
      /* x is NaN or infinite. */
      return x + x;
    else
      return x;
  } else {
    uint32_t exponent_mask = 0xffffffff >> (exponent_less_1023 - 20);
    uint32_t tmp;

    if ((lsw & exponent_mask) == 0)
      /* x is an integral value. */
      return x;

    tmp = lsw + (1 << (51 - exponent_less_1023));
    if (tmp < lsw)
      msw += 1;
    lsw = tmp;

    lsw &= ~exponent_mask;
  }
  INSERT_WORDS(x, msw, lsw);

  return x;
}