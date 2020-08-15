#ifndef _STDDEF_H
#define _STDDEF_H 1
#include <stdint.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

/* size_t is used for memory object sizes */
typedef uintptr_t size_t;

#endif
