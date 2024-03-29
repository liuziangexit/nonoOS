#ifndef __KERNEL_INTERRUPT_H__
#define __KERNEL_INTERRUPT_H__
#include <defs.h>

/* Trap Numbers */

/* Processor-defined: */
#define T_DIVIDE 0 // divide error
#define T_DEBUG 1  // debug exception
#define T_NMI 2    // non-maskable interrupt
#define T_BRKPT 3  // breakpoint
#define T_OFLOW 4  // overflow
#define T_BOUND 5  // bounds check
#define T_ILLOP 6  // illegal opcode
#define T_DEVICE 7 // device not available
#define T_DBLFLT 8 // double fault
// #define T_COPROC             9   // reserved (not used since 486)
#define T_TSS 10   // invalid task switch segment
#define T_SEGNP 11 // segment not present
#define T_STACK 12 // stack exception
#define T_GPFLT 13 // general protection fault
#define T_PGFLT 14 // page fault
// #define T_RES                15  // reserved
#define T_FPERR 16   // floating point error
#define T_ALIGN 17   // aligment check
#define T_MCHK 18    // machine check
#define T_SIMDERR 19 // SIMD floating point error

/* Hardware IRQ numbers. We receive these as (IRQ_OFFSET + IRQ_xx) */
#define IRQ_OFFSET 32 // IRQ 0 corresponds to int IRQ_OFFSET

#define IRQ_TIMER 0
#define IRQ_KBD 1
#define IRQ_COM1 4
#define IRQ_IDE1 14
#define IRQ_IDE2 15
#define IRQ_ERROR 19
#define IRQ_SPURIOUS 31

// 软中断
#define T_SWITCH_KERNEL 120 //用户切到内核
#define T_SWITCH_USER 121   //内核切到用户
#define T_SYSCALL 122       //系统调用

/* pushal储存的General-purpose registers */
struct gprs {
  uint32_t reg_edi;
  uint32_t reg_esi;
  uint32_t reg_ebp;
  uint32_t reg_oesp; /* Useless */
  uint32_t reg_ebx;
  uint32_t reg_edx;
  uint32_t reg_ecx;
  uint32_t reg_eax;
} __attribute__((packed));

struct trapframe_kernel {
  struct gprs gprs;
  uint16_t gs;
  uint16_t padding0;
  uint16_t fs;
  uint16_t padding1;
  uint16_t es;
  uint16_t padding2;
  uint16_t ds;
  uint16_t padding3;
  uint32_t trapno;
  /* below here are pushed by x86 hardware */
  uint32_t err;
  uintptr_t eip;
  uint16_t cs;
  uint16_t padding4;
  uint32_t eflags;
} __attribute__((packed));

struct trapframe {
  struct gprs gprs;
  uint16_t gs;
  uint16_t padding0;
  uint16_t fs;
  uint16_t padding1;
  uint16_t es;
  uint16_t padding2;
  uint16_t ds;
  uint16_t padding3;
  uint32_t trapno;
  /* below here are pushed by x86 hardware */
  uint32_t err;
  uintptr_t eip;
  uint16_t cs;
  uint16_t padding4;
  uint32_t eflags;
  /* below here only when crossing rings, such as from user to kernel */
  // int指令发生的时候，处理器会检查RPL(由IDT里gate的seg指示)是否小于CPL
  //如果小于的话 1.将CPL切到RPL  2.使用TSS里指定的RPL(ring0)栈  3.会额外push下面8字节的数据
  uintptr_t esp;
  uint16_t ss;
  uint16_t padding5;
} __attribute__((packed));

void idt_init(void);
void print_trapframe(struct trapframe *tf);
void print_regs(struct gprs *regs);
bool trap_in_kernel(struct trapframe *tf);

#endif /* !__KERN_TRAP_TRAP_H__ */
