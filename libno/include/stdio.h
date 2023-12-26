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
size_t
getslen(); // 获取下一行的长度（包含\0），如果没有找到行结束标志会阻塞等待
char *gets(char *str);
int getchar();

#ifndef LIBNO_USER
int printf_color(enum cga_color color, const char *restrict fmt, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif
