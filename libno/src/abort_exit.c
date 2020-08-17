#include <stdlib.h>
#include <panic.h>

void abort(void) {
  panic("abort has been called");
}

void exit(int ret){
  panic("guaguaguagua");
}
