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
int putchar(int);
int puts(const char *);
int printf(const char *format, ...);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t count, const char *format, ...);

// 这个文件无法包含stdarg.h，一旦包含就要报错，好奇怪
// int vsnprintf(char *buffer, size_t count, const char *format, va_list va);
// int vprintf(const char *format, va_list va);

// int fctprintf(void (*out)(char character, void *arg), void *arg,
//               const char *format, ...);

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
