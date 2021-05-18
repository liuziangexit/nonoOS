#include <assert.h>
#include <compiler_helper.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

#ifndef LIBNO_USER
#include <tty.h>

int putchar(int ic) {
  char c = (char)ic;
  terminal_putchar(c);
  return ic;
}

static void print(const char *data, size_t length) {
  terminal_write(data, length);
}

#define GET_ARG(TYPE) (va_arg(*parameters, TYPE))
// TODO 瞎xx写的printf，以后重构掉或者抄一个来
int printf_impl(const char *restrict format, va_list *parameters) {
  int written = 0;
  while (*format != '\0') {
    size_t maxrem = INT_MAX - written;
    if (format[0] != '%' || format[1] == '%') {
      if (format[0] == '%')
        format++;
      size_t amount = 1;
      while (format[amount] && format[amount] != '%')
        amount++;
      if (maxrem < amount) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      print(format, amount);
      format += amount;
      written += amount;
      continue;
    }

    const char *format_begun_at = format++;

    if (*format == 'c') {
      format++;
      char c = (char)GET_ARG(int);
      if (!maxrem) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      print(&c, sizeof(c));
      written++;
    } else if (*format == 's') {
      format++;
      const char *str = GET_ARG(const char *);
      size_t len = strlen(str);
      if (maxrem < len) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      print(str, len);
      written += len;
    } else if (strlen(format) >= 2 && *(format) == 'l' &&
               *(format + 1) == 'l') {
      format += 2;
      int64_t v = GET_ARG(uint64_t);
      if (!maxrem) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      static char sv[30];
      if (ltoa(v, sv, 30, 10))
        return -1; // itoa failed
      size_t len = strlen(sv);
      print(sv, len);
      written += len;
    } else if (*format == 'd') {
      format++;
      int32_t v = GET_ARG(int32_t);
      if (!maxrem) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      static char sv[10];
      if (itoa(v, sv, 10, 10))
        return -1; // itoa failed
      size_t len = strlen(sv);
      print(sv, len);
      written += len;
    } else if (strlen(format) >= 3 && isdigit(*format) &&
               isdigit(*(format + 1)) && *(format + 2) == 'x') {
      char placeholder = *format;
      format++;
      uint32_t digit_cnt = *format - '0';
      format++;
      format++; // skip x
      int32_t v = GET_ARG(int32_t);
      if (!maxrem) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      static char sv[11];
      if (itoa(v, sv, 11, 16))
        return -1; // itoa failed
      size_t len = strlen(sv);
      if (len < digit_cnt) {
        int ph_add = digit_cnt - len;
        if (len + ph_add <= sizeof(sv) - 1) {
          memmove(sv + ph_add, sv, len);
          memset(sv, placeholder, ph_add);
          len += ph_add;
          sv[len] = 0;
        }
      }
      print(sv, len);
      written += len;
    } else if (strlen(format) >= 5 && isdigit(*format) &&
               isdigit(*(format + 1)) && *(format + 2) == 'l' &&
               *(format + 3) == 'l' && *(format + 4) == 'x') {
      char placeholder = *format;
      format++;
      uint64_t digit_cnt = *format - '0';
      format++;
      format += 3; // skip llx
      int64_t v = GET_ARG(int64_t);
      if (!maxrem) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      static char sv[24];
      if (ltoa(v, sv, 24, 16))
        return -1; // itoa failed
      size_t len = strlen(sv);
      if (len < digit_cnt) {
        int ph_add = digit_cnt - len;
        if (len + ph_add <= sizeof(sv) - 1) {
          memmove(sv + ph_add, sv, len);
          memset(sv, placeholder, ph_add);
          len += ph_add;
          sv[len] = 0;
        }
      }
      print(sv, len);
      written += len;
    } else {
      format = format_begun_at;
      size_t len = strlen(format);
      if (maxrem < len) {
        // TODO: Set errno to EOVERFLOW.
        return -1;
      }
      print(format, len);
      written += len;
      format += len;
    }
  }
  return written;
}

int printf(const char *restrict format, ...) {
  va_list parameters;
  va_start(parameters, format);
  int ret = printf_impl(format, &parameters);
  va_end(parameters);
  return ret;
}

int puts(const char *string) { return printf("%s\n", string); }

#else

#include <syscall.h>

int putchar(int ic) {
  printf("%c", ic);
  return ic;
}

int printf(const char *restrict format, ...) {
  va_list parameters;
  va_start(parameters, format);
  uint32_t ret = syscall(SYSCALL_PRINTF, 2, format, &parameters);
  va_end(parameters);
  return ret;
}

int puts(const char *string) { return printf("%s\n", string); }

#endif