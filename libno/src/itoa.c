#include <stdlib.h>
#include <string.h>

int itoa(uint32_t num, unsigned char *str, uint32_t len, uint32_t base) {
  uint32_t sum = num;
  uint32_t i = 0;
  uint32_t digit;
  if (len == 0)
    return -1;
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
  str[i] = '\0';
  strrev(str);
  return 0;
}