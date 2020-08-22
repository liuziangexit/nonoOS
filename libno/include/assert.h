#ifndef __LIBNO_ASSERT_H__
#define __LIBNO_ASSERT_H__
#include <stdlib.h>

#ifdef NDEBUG
#define assert(expr)
#else
#define assert(expr)                                                              \
  if (!(expr)) {                                                                    \
    abort();                                                                   \
  }
#endif

#endif
