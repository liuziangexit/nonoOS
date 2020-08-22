#ifndef _ASSERT_H
#define _ASSERT_H 1
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
