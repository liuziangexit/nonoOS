#ifndef __LIBNO_STDDEF_H_
#define __LIBNO_STDDEF_H_
#include <stdint.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

/* size_t is used for memory object sizes */
typedef uintptr_t size_t;
typedef uintptr_t ptrdiff_t;

#endif
