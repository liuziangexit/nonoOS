#include <sync.h>
#include <x86.h>

void disable_interrupt() { cli(); }
void enable_interrupt() { sti(); }
