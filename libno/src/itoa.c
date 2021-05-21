#include <stdlib.h>
#include <string.h>

int itoa(int32_t num, char *str, uint32_t len, uint32_t base) {
  uint32_t sum;
  if (num < 0)
    sum = 0 - num;
  else
    sum = num;
  uint32_t i = 0;
  uint32_t digit;
  if (len == 0) {
    return -1;
  }
  do {
    digit = sum % base;
    if (digit < 0xA)
      str[i++] = '0' + digit;
    else
      str[i++] = 'a' + digit - 0xA;
    sum /= base;
  } while (sum && (i < (len - 1)));
  if (i == (len - 1) && sum)
    return -1;
  if (num < 0) {
    str[i++] = '-';
  }
  str[i] = '\0';
  strrev(str);
  return 0;
}

int ltoa(int64_t num, char *str, uint32_t len, uint32_t base) {
  uint64_t sum;
  if (num < 0)
    sum = 0 - num;
  else
    sum = num;
  uint64_t i = 0;
  uint64_t digit;
  if (len == 0) {
    return -1;
  }
  do {
    digit = sum % base;
    if (digit < 0xA)
      str[i++] = '0' + digit;
    else
      str[i++] = 'a' + digit - 0xA;
    sum /= base;
  } while (sum && (i < (len - 1)));
  if (i == (len - 1) && sum)
    return -1;
  if (num < 0) {
    str[i++] = '-';
  }
  str[i] = '\0';
  strrev(str);
  return 0;
}
