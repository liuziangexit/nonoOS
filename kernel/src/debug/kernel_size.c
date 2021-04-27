#include <debug.h>
#include <stdint.h>
#include <stdio.h>

void print_kernel_size() {
  extern char program_end[], program_begin[];
  extern char text_end[], text_begin[];
  extern char rodata_end[], rodata_begin[];
  extern char data_end[], data_begin[];
  extern char bss_end[], bss_begin[];
  int32_t program_size = (int32_t)(program_end - program_begin);
  int32_t text_size = (int32_t)(text_end - text_begin);
  int32_t rodata_size = (int32_t)(rodata_end - rodata_begin);
  int32_t data_size = (int32_t)(data_end - data_begin);
  int32_t bss_size = (int32_t)(bss_end - bss_begin);
  printf("kernel image size: %d bytes (%d KB)\n", program_size,
         program_size / 1024);
  printf("text section size: %d bytes (%d KB)\n", text_size, text_size / 1024);
  printf("rodata section size: %d bytes (%d KB)\n", rodata_size,
         rodata_size / 1024);
  printf("data section size: %d bytes (%d KB)\n", data_size, data_size / 1024);
  printf("bss section size: %d bytes (%d KB)\n", bss_size, bss_size / 1024);
}