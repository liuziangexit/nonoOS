#include <assert.h>
#include <defs.h>
#include <interrupt.h>
#include <kbd.h>
#include <memlayout.h>
#include <mmu.h>
#include <panic.h>
#include <stdio.h>
#include <x86.h>

/* *
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 * */
static struct gatedesc idt[256] = {{0}};

static struct pseudodesc idt_pd = {sizeof(idt) - 1, (uint32_t)idt};

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
 */
void idt_init(void) {
  extern uintptr_t __idt_vectors[];
  unsigned int i;
  for (i = 0; i < sizeof(idt) / sizeof(struct gatedesc); i++) {
    SETGATE(idt[i], 0, SEG_KCODE, __idt_vectors[i], DPL_KERNEL);
  }

  // load the IDT
  lidt(&idt_pd);
}

static const char *trapname(unsigned int trapno) {
  static const char *const excnames[] = {"Divide error",
                                         "Debug",
                                         "Non-Maskable Interrupt",
                                         "Breakpoint",
                                         "Overflow",
                                         "BOUND Range Exceeded",
                                         "Invalid Opcode",
                                         "Device Not Available",
                                         "Double Fault",
                                         "Coprocessor Segment Overrun",
                                         "Invalid TSS",
                                         "Segment Not Present",
                                         "Stack Fault",
                                         "General Protection",
                                         "Page Fault",
                                         "(unknown trap)",
                                         "x87 FPU Floating-Point Error",
                                         "Alignment Check",
                                         "Machine-Check",
                                         "SIMD Floating-Point Exception"};

  if (trapno < sizeof(excnames) / sizeof(const char *const)) {
    return excnames[trapno];
  }
  if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
    return "Hardware Interrupt";
  }
  return "(unknown trap)";
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trapframe *tf) {
  return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static __always_inline uintptr_t rcr2(void) {
  uintptr_t cr2;
  asm volatile("mov %%cr2, %0" : "=r"(cr2)::"memory");
  return cr2;
}

static inline void print_pgfault(struct trapframe *tf) {
  /* error_code:
   * bit 0 == 0 means no page found, 1 means protection fault
   * bit 1 == 0 means read, 1 means write
   * bit 2 == 0 means kernel, 1 means user
   * */
  printf("page fault at 0x%08x: %c/%c [%s].\n", rcr2(),
         (tf->tf_err & 4) ? 'U' : 'K', (tf->tf_err & 2) ? 'W' : 'R',
         (tf->tf_err & 1) ? "protection fault" : "no page found");
}

/* temporary trapframe or pointer to trapframe */
struct trapframe switchk2u, *switchu2k;

/* trap_dispatch - dispatch based on what type of trap occurred */
void interrupt_handler(struct trapframe *tf) {
  switch (tf->tf_trapno) {
  case IRQ_OFFSET + IRQ_KBD:
    kbd_isr();
    break;
  // TODO 看看为啥switchk2u需要拷贝，而switchu2k只是指针就够了
  case T_SWITCH_USER:
    if (tf->tf_cs != USER_CS) {
      switchk2u = *tf;
      switchk2u.tf_cs = USER_CS;
      switchk2u.tf_ds = switchk2u.tf_es = USER_DS;
      switchk2u.tf_ss = USER_DS;
      // set eflags, make sure ucore can use io under user mode.
      // if CPL > IOPL, then cpu will generate a general protection.
      switchk2u.tf_eflags |= FL_IOPL_MASK;

      switchk2u.tf_esp = (uint32_t)tf + sizeof(struct trapframe) - 8;

      // set temporary stack
      // then iret will jump to the right stack
      *((uint32_t *)tf - 1) = (uint32_t)&switchk2u;
    }
    break;
  case T_SWITCH_KERNEL:
    if (tf->tf_cs != KERNEL_CS) {
      tf->tf_cs = KERNEL_CS;
      tf->tf_ds = tf->tf_es = KERNEL_DS;
      tf->tf_eflags &= ~FL_IOPL_MASK;
      // FIXME 下面这两行是不是在做无用功？看起来是的！
      switchu2k =
          (struct trapframe *)(tf->tf_esp - (sizeof(struct trapframe) - 8));
      memmove(switchu2k, tf, sizeof(struct trapframe) - 8);
      *((uint32_t *)tf - 1) = (uint32_t)switchu2k;
    }
    break;
  case T_PGFLT: {
    print_pgfault(tf);
    panic("Page Fault?\n");
  }
  default:
    if ((tf->tf_cs & 3) == 0) {
      // in kernel, it must be a mistake
      /*
      TODO
      这里我想打印一下trap的number，所以在这里必须用一个sprintf去组装一个字符串，
      然后再丢到panic里面，但是printf那边还比较乱，没有办法搞一个sprintf出来
      必须先重构printf那块，做出sprintf，然后再改这个地方
      */
      printf("%s\n", trapname(tf->tf_trapno));
      panic("unexpected trap in kernel.\n");
    }
  }
}
