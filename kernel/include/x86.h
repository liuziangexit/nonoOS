#ifndef __KERNEL_X86_H__
#define __KERNEL_X86_H__

#include <assert.h>
#include <compiler_helper.h>
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

//这函数必须是inline，否则它的栈帧会导致esp与ebp的移动！
static inline __always_inline void rebp(uint32_t *ret) {
  asm volatile("movl %%ebp, %0" : "=r"(*ret));
}

//这函数必须是inline，否则它的栈帧会导致esp与ebp的移动！
static inline __always_inline void resp(uint32_t *ret) {
  asm volatile("movl %%esp, %0" : "=r"(*ret));
}

static inline __always_inline void lebp(uint32_t ebp) {
  asm volatile("movl %0, %%ebp" ::"r"(ebp) : "memory");
}

static inline __always_inline void lesp(uint32_t esp) {
  asm volatile("movl %0, %%esp" ::"r"(esp) : "memory");
}

static inline __always_inline void lidt(struct pseudodesc *pd) {
  asm volatile("lidt (%0)" ::"r"(pd));
}

static inline __always_inline void sti(void) { asm volatile("sti" ::: "cc"); }

static inline __always_inline void cli(void) { asm volatile("cli" ::: "cc"); }

static __always_inline uint32_t reflags() {
  uint32_t eflags;
  asm("pushfl; popl %0" : "=r"(eflags));
  return eflags;
}

static __always_inline void weflags(uint32_t eflags) {
  asm("pushl %0; popfl" ::"r"(eflags) : "cc");
}

static inline __always_inline void ltr(uint16_t sel) {
  asm volatile("ltr %0" ::"r"(sel));
}

static __always_inline void lcr2(uintptr_t cr2) {
  asm volatile("mov %0, %%cr2" ::"r"(cr2) : "memory");
}

static __always_inline uintptr_t rcr2(void) {
  uintptr_t cr2;
  asm volatile("mov %%cr2, %0" : "=r"(cr2)::"memory");
  return cr2;
}

static __always_inline void lcr3(uintptr_t cr3) {
  assert(cr3 % 4096 == 0);
  asm volatile("mov %0, %%cr3" ::"r"(cr3) : "memory");
}

static __always_inline uintptr_t rcr3() {
  uintptr_t val;
  asm volatile("mov %%cr3, %0" : "=r"(val)::"memory");
  return val;
}

static __always_inline void lcr4(uintptr_t cr4) {
  asm volatile("mov %0, %%cr4" ::"r"(cr4) : "memory");
}

static __always_inline uintptr_t rcr4() {
  uintptr_t val;
  asm volatile("mov %%cr4, %0" : "=r"(val)::"memory");
  return val;
}

static __always_inline void lcr0(uintptr_t cr0) {
  asm volatile("mov %0, %%cr0" ::"r"(cr0) : "memory");
}

static __always_inline uintptr_t rcr0() {
  uintptr_t val;
  asm volatile("mov %%cr0, %0" : "=r"(val)::"memory");
  return val;
}

#endif /* !__LIBS_X86_H__ */
