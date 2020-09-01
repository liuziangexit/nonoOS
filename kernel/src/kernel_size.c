#include <debug.h>
#include <stdint.h>
#include <stdio.h>

void print_kernel_size() {
  extern char kentry[], bss_end[];
  printf("kernel image size: %d bytes (%d KB)\n", (int32_t)(bss_end - kentry),
         (int32_t)(bss_end - kentry) / 1024);
}