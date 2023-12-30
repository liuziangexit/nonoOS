#ifndef __KERNEL_CTL_CHAR_HANDLER_H__
#define __KERNEL_CTL_CHAR_HANDLER_H__
#include <stdint.h>

void control_character_handler(int32_t *c, uint32_t shift);

#endif