#include <panic.h>
#include <stdlib.h>
#include <compiler_helper.h>

void abort(void) { panic("abort has been called"); }

void exit(int ret) {
  UNUSED(ret);
  panic("guaguaguagua");
}
