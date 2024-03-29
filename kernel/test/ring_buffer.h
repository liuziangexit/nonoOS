#include <assert.h>
#include <ring_buffer.h>
#include <stdio.h>
#include <string.h>
#include <tty.h>

#ifdef NDEBUG
#define __NDEBUG_BEEN_FUCKED
#undef NDEBUG
#endif

void ring_buffer_test() {
  printf("running ring_buffer_test\n");
  struct ring_buffer buffer;
  char b[5], tmp[5];
  const char *label = "wjss";
  ring_buffer_init(&buffer, b, 5);
  assert(!ring_buffer_write(&buffer, false, label, 5));
  assert(ring_buffer_write(&buffer, false, label, 4));
  assert(ring_buffer_readable(&buffer) == 4);
  assert(!ring_buffer_write(&buffer, false, label, 1));
  assert(4 == ring_buffer_read(&buffer, tmp, 5));
  assert(!memcmp(label, tmp, 4));
  assert(ring_buffer_readable(&buffer) == 0);
  assert(0 == ring_buffer_read(&buffer, tmp, 5));

  assert(ring_buffer_write(&buffer, false, label, 2));
  assert(ring_buffer_readable(&buffer) == 2);
  assert(1 == ring_buffer_read(&buffer, tmp, 1));
  assert(ring_buffer_readable(&buffer) == 1);
  assert(1 == ring_buffer_read(&buffer, tmp + 1, 1));
  assert(!memcmp(label, tmp, 2));
  assert(ring_buffer_readable(&buffer) == 0);
  assert(0 == ring_buffer_read(&buffer, tmp, 5));

  assert(!ring_buffer_write(&buffer, false, label, 5));
  assert(ring_buffer_write(&buffer, false, label, 4));
  assert(ring_buffer_readable(&buffer) == 4);
  assert(!ring_buffer_write(&buffer, false, label, 1));
  assert(4 == ring_buffer_read(&buffer, tmp, 5));
  assert(!memcmp(label, tmp, 4));
  assert(ring_buffer_readable(&buffer) == 0);
  assert(0 == ring_buffer_read(&buffer, tmp, 5));
  ring_buffer_init(&buffer, b, 5);
  assert(!ring_buffer_write(&buffer, true, "123456", 5));
  assert(ring_buffer_write(&buffer, true, "123456", 4));
  assert(ring_buffer_write(&buffer, true, "123456", 4));
  assert(4 == ring_buffer_read(&buffer, tmp, 4));
  assert(!memcmp("12345", tmp, 4));
  assert(ring_buffer_write(&buffer, false, "1234", 4));
  assert(ring_buffer_write(&buffer, true, "12", 2));
  assert(4 == ring_buffer_read(&buffer, tmp, 4));
  assert(!memcmp("3412", tmp, 4));

  // b test/ring_buffer.h:56
  assert(ring_buffer_write(&buffer, false, "1234", 4));
  assert(ring_buffer_write(&buffer, true, "12", 2));
  ring_buffer_copyout(&buffer, 0, 4, tmp);
  assert(!memcmp("3412", tmp, 4));
  assert(ring_buffer_write(&buffer, true, "12", 2));
  ring_buffer_copyout(&buffer, 0, 4, tmp);
  assert(!memcmp("1212", tmp, 4));

  terminal_color(CGA_COLOR_GREEN, CGA_COLOR_GREEN);
  printf(" ");
  terminal_default_color();
  printf("ring_buffer_test passed!!!\n");
  terminal_default_color();
}

#ifdef __NDEBUG_BEEN_FUCKED
#define NDEBUG
#undef __NDEBUG_BEEN_FUCKED
#endif