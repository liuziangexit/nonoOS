#ifndef __LIBNO_STDLIB_H__
#define __LIBNO_STDLIB_H__
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void abort();
void exit(int);

//将数字转为其字符串表示
int itoa(uint32_t num, char *str, uint32_t len, uint32_t base);
int ltoa(uint64_t num, char *str, uint32_t len, uint32_t base);

//堆内存
void *malloc(size_t size);
void free(void *p);

#ifdef __cplusplus
}
#endif

#endif
