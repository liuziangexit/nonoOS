#include <stdint.h>
#include <string.h>

// TODO SIMD

int memcmp(const void *aptr, const void *bptr, size_t size) {
  const unsigned char *a = (const unsigned char *)aptr;
  const unsigned char *b = (const unsigned char *)bptr;
  for (size_t i = 0; i < size; i++) {
    if (a[i] < b[i])
      return -1;
    else if (b[i] < a[i])
      return 1;
  }
  return 0;
}

void *memcpy(void *restrict dstptr, const void *restrict srcptr, size_t size) {
  unsigned char *dst = (unsigned char *)dstptr;
  const unsigned char *src = (const unsigned char *)srcptr;
  for (size_t i = 0; i < size; i++)
    dst[i] = src[i];
  return dstptr;
}

void *memmove(void *dstptr, const void *srcptr, size_t size) {
  unsigned char *dst = (unsigned char *)dstptr;
  const unsigned char *src = (const unsigned char *)srcptr;
  if (dst < src) {
    for (size_t i = 0; i < size; i++)
      dst[i] = src[i];
  } else {
    for (size_t i = size; i != 0; i--)
      dst[i - 1] = src[i - 1];
  }
  return dstptr;
}

void *memset(void *bufptr, int value, size_t size) {
  unsigned char *buf = (unsigned char *)bufptr;
  for (size_t i = 0; i < size; i++)
    buf[i] = (unsigned char)value;
  return bufptr;
}

size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len])
    len++;
  return len;
}

void strrev(char *str) {
  int i;
  int j;
  unsigned char a;
  unsigned len = strlen((const char *)str);
  for (i = 0, j = len - 1; i < j; i++, j--) {
    a = str[i];
    str[i] = str[j];
    str[j] = a;
  }
}

char *strcpy(char *dst, const char *src) {
  uint32_t len = strlen(src);
  memcpy(dst, src, len);
  return dst;
}

// 从glibc偷来的
/* Compare S1 and S2, returning less than, equal to or
   greater than zero if S1 is lexicographically less than,
   equal to or greater than S2.  */
int strcmp(const char *p1, const char *p2) {
  const unsigned char *s1 = (const unsigned char *)p1;
  const unsigned char *s2 = (const unsigned char *)p2;
  unsigned char c1, c2;
  do {
    c1 = (unsigned char)*s1++;
    c2 = (unsigned char)*s2++;
    if (c1 == '\0')
      return c1 - c2;
  } while (c1 == c2);
  return c1 - c2;
}
