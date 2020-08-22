#ifndef __LIBNO_STDBOOL_H__
#define __LIBNO_STDBOOL_H__
#include <stdint.h>

/* booleans */
#ifndef __bool_true_false_are_defined
typedef int32_t bool;
#define true 1
#define false 0
#define __bool_true_false_are_defined
#endif

#endif
