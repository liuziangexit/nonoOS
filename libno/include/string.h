#ifndef __LIBNO_STRING_H__
#define __LIBNO_STRING_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int memcmp(const void *, const void *, size_t);
void *memcpy(void *__restrict, const void *__restrict, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
size_t strlen(const char *);
void strrev(char *str);

#ifdef __cplusplus
}
#endif

#endif
