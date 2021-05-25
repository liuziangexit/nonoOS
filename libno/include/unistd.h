#ifndef __LIBNO_UNISTD_H__
#define __LIBNO_UNISTD_H__
#ifdef LIBNO_USER
#include <stdint.h>
void sleep(uint64_t ms);
#endif
#endif
