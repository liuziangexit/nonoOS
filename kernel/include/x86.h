#ifndef __KERNEL_X86_H__
#define __KERNEL_X86_H__

#include <defs.h>

/* Pseudo-descriptors used for LGDT, LLDT(not used) and LIDT instructions. */
struct pseudodesc {
  uint16_t pd_lim;  // Limit
  uint32_t pd_base; // Base address
} __attribute__((packed));

static inline __always_inline void hlt() { asm volatile("hlt" : :); }

static inline __always_inline uint8_t inb(uint16_t port) {
  uint8_t data;
  asm volatile("inb %1, %0" : "=a"(data) : "d"(port));
  return data;
}

static inline __always_inline void insl(uint32_t port, void *addr, int cnt) {
  asm volatile("cld;"
               "repne; insl;"
               : "=D"(addr), "=c"(cnt)
               : "d"(port), "0"(addr), "1"(cnt)
               : "memory", "cc");
}

static inline __always_inline void outb(uint16_t port, uint8_t data) {
  asm volatile("outb %0, %1" ::"a"(data), "d"(port));
}

static inline __always_inline void outw(uint16_t port, uint16_t data) {
  asm volatile("outw %0, %1" ::"a"(data), "d"(port));
}

static inline __always_inline uint32_t read_ebp(void) {
  uint32_t ebp;
  asm volatile("movl %%ebp, %0" : "=r"(ebp));
  return ebp;
}

static inline __always_inline void lidt(struct pseudodesc *pd) {
  asm volatile("lidt (%0)" ::"r"(pd));
}

static inline __always_inline void sti(void) { asm volatile("sti"); }

static inline __always_inline void cli(void) { asm volatile("cli"); }

static inline __always_inline void ltr(uint16_t sel) {
  asm volatile("ltr %0" ::"r"(sel));
}

static __always_inline void lcr3(uintptr_t cr3) {
  asm volatile("mov %0, %%cr3" ::"r"(cr3) : "memory");
}

static __always_inline uintptr_t rcr3(void) {
  uintptr_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3)::"memory");
  return cr3;
}

#endif /* !__LIBS_X86_H__ */
