#include <reg_info.h>
#include <stdint.h>

void print_cur_status() {
  uint16_t reg[6];
  uint32_t exx[2];
  asm volatile("mov %%cs, %0;"
               "mov %%ds, %1;"
               "mov %%es, %2;"
               "mov %%fs, %3;"
               "mov %%gs, %4;"
               "mov %%ss, %5;"
               "movl %%esp, %6;"
               "movl %%ebp, %7;"
               : "=m"(reg[0]), "=m"(reg[1]), "=m"(reg[2]), "=m"(reg[3]),
                 "=m"(reg[4]), "=m"(reg[5]), "=m"(exx[0]), "=m"(exx[1]));
  printf("current status:");
  printf("ring %d, cs = %02x, ds = %02x, es = %02x, fs = %02x, gs = %02x, ss = "
         "%02x\nesp = %02x, ebp = %02x\n",
         reg[0] & 3, reg[0], reg[1], reg[2], reg[3], reg[4], reg[5], exx[0],
         exx[1]);
}
