#ifndef _STDBOOL_H
#define _STDBOOL_H 1
#include <stdint.h>

/* booleans */
#ifndef __bool_true_false_are_defined
typedef int32_t bool;
#define true 1
#define false 0
#define __bool_true_false_are_defined
#endif;

#endif
