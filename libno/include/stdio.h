#ifndef _STDIO_H
#define _STDIO_H 1

#include <defs.h>
#include <sys/cdefs.h>

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
