#ifndef __KERNEL_DEFS_H__
#define __KERNEL_DEFS_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* *
 * Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n
 * */
#define ROUNDDOWN(a, n)                                                        \
  ({                                                                           \
    size_t __a = (size_t)(a);                                                  \
    (typeof(a))(__a - __a % (n));                                              \
  })

/* Round up to the nearest multiple of n */
#define ROUNDUP(a, n)                                                          \
  ({                                                                           \
    size_t __n = (size_t)(n);                                                  \
    (typeof(a))(ROUNDDOWN((size_t)(a) + __n - 1, __n));                        \
  })

#endif /* !__LIBS_DEFS_H__ */
