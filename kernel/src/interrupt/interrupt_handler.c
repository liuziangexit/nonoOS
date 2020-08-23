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

static const char *IA32flags[] = {
    "CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",  "TF",  "IF", "DF", "OF",
    NULL, NULL, "NT", NULL, "RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void print_trapframe(struct trapframe *tf) {
  printf("trapframe at %p\n", tf);
  print_regs(&tf->tf_gprs);
  printf("  ds   0x----%04x\n", tf->tf_ds);
  printf("  es   0x----%04x\n", tf->tf_es);
  printf("  fs   0x----%04x\n", tf->tf_fs);
  printf("  gs   0x----%04x\n", tf->tf_gs);
  printf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
  printf("  err  0x%08x\n", tf->tf_err);
  printf("  eip  0x%08x\n", tf->tf_eip);
  printf("  cs   0x----%04x\n", tf->tf_cs);
  printf("  flag 0x%08x ", tf->tf_eflags);

  int i, j;
  for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]);
       i++, j <<= 1) {
    if ((tf->tf_eflags & j) && IA32flags[i] != NULL) {
      printf("%s,", IA32flags[i]);
    }
  }
  printf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

  if (!trap_in_kernel(tf)) {
    printf("  esp  0x%08x\n", tf->tf_esp);
    printf("  ss   0x----%04x\n", tf->tf_ss);
  }
}

void print_regs(struct gprs *regs) {
  printf("  edi  0x%08x\n", regs->reg_edi);
  printf("  esi  0x%08x\n", regs->reg_esi);
  printf("  ebp  0x%08x\n", regs->reg_ebp);
  printf("  oesp 0x%08x\n", regs->reg_oesp);
  printf("  ebx  0x%08x\n", regs->reg_ebx);
  printf("  edx  0x%08x\n", regs->reg_edx);
  printf("  ecx  0x%08x\n", regs->reg_ecx);
  printf("  eax  0x%08x\n", regs->reg_eax);
}

/* trap_dispatch - dispatch based on what type of trap occurred */
static void trap_dispatch(struct trapframe *tf) {
  switch (tf->tf_trapno) {
  case IRQ_OFFSET + IRQ_TIMER:
    /*
      ticks++;
      if (ticks % TICK_NUM == 0) {
        print_ticks();
      }
      */
    break;
  case IRQ_OFFSET + IRQ_KBD:
    kbd_isr();
    break;
  default:
    // in kernel, it must be a mistake
    if ((tf->tf_cs & 3) == 0) {
      print_trapframe(tf);
      panic("unexpected trap in kernel.\n");
    }
  }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns, the code in kern/trap/trapentry.S restores the old CPU state saved
 * in the trapframe and then uses the iret instruction to return from the
 * exception.
 * */
void trap(struct trapframe *tf) { trap_dispatch(tf); }
