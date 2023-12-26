#ifndef __LIBNO_STDIO_H__
#define __LIBNO_STDIO_H__

#ifndef LIBNO_USER
#include "cga.h"
#endif
#include <stddef.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

// out
int printf(const char *restrict fmt, ...);
int putchar(int);
int puts(const char *);

// in
#ifndef LIBNO_USER

// str_size指示str的长度
// 返回值<=str_len表示成功
// 返回值>str_len表示因为str长度太短失败，返回值指示str最少应该具有的长度
size_t kgets(char *str, size_t str_len);

#else
char *gets(char *str);
#endif
int getchar();

#ifndef LIBNO_USER
int printf_color(enum cga_color color, const char *restrict fmt, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif
