#ifndef __LIBNO_STDLIB_H__
#define __LIBNO_STDLIB_H__
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void abort(void);
void exit(int);

//将数字转为其字符串表示
int itoa(uint32_t num, char *str, uint32_t len, uint32_t base);

#ifdef __cplusplus
}
#endif

#endif
