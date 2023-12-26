#ifndef __LIBNO_STDIO_H__
#define __LIBNO_STDIO_H__

#ifndef LIBNO_USER
#include "cga.h"
#endif

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

// out
int printf(const char *restrict fmt, ...);
int putchar(int);
int puts(const char *);

// in
char *gets(char *str);
int getchar();

#ifndef LIBNO_USER
int printf_color(enum cga_color color, const char *restrict fmt, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif
