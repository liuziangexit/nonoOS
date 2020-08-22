#ifndef __LIBNO_STDIO_H__
#define __LIBNO_STDIO_H__

#include <defs.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

// out
int printf(const char *__restrict, ...);
int putchar(int);
int puts(const char *);

// in
char *gets(char *str);
int getchar();

#ifdef __cplusplus
}
#endif

#endif
