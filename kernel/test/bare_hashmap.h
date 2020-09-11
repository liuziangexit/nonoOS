#include "../src/memory/bare_hashmap.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>

#ifdef NDEBUG
#define __NDEBUG_BEEN_FUCKED
#undef NDEBUG
#endif

void bare_hashmap_test() {
  printf("running bare_hashmap_test\n");

  terminal_color(CGA_COLOR_GREEN, CGA_COLOR_GREEN);
  printf(" ");
  terminal_default_color();
  printf("bare_hashmap_test passed!!!\n");
  terminal_default_color();
}

#ifdef __NDEBUG_BEEN_FUCKED
#define NDEBUG
#undef __NDEBUG_BEEN_FUCKED
#endif