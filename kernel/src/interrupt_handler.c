#include <assert.h>
#include <defs.h>
#include <interrupt.h>
#include <kbd.h>
#include <memlayout.h>
#include <mmu.h>
#include <panic.h>
#include <stdio.h>
#include <string.h>
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

  //用户态转内核态的中断的特权级改为最低
  SETGATE(idt[T_SWITCH_KERNEL], 0, SEG_KCODE, __idt_vectors[T_SWITCH_KERNEL],
          DPL_USER);

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

static struct trapframe switchk2u, *switchu2k;

/* trap_dispatch - dispatch based on what type of trap occurred */
void interrupt_handler(struct trapframe *tf) {
  switch (tf->tf_trapno) {
  case IRQ_OFFSET + IRQ_KBD:
    kbd_isr();
    break;
  case T_SWITCH_USER:
    if (tf->tf_cs != USER_CS) {
      //将tf的内容拷贝到switchk2u，然后把寄存器们都改成用户权限
      switchk2u = *tf;
      switchk2u.tf_cs = USER_CS;
      switchk2u.tf_ds = switchk2u.tf_es = USER_DS;
      switchk2u.tf_ss = USER_DS;
      // set eflags, make sure ucore can use io under user mode.
      // if CPL > IOPL, then cpu will generate a general protection.
      switchk2u.tf_eflags |= FL_IOPL_MASK;

      /*
      为了解决下面说的esp错误指向switchk2u附近的问题，我们在这里把switchk2u.tf_esp设置为esp应该指向的正确位置，也就是tf之前()
      (“tf之前”是从栈的角度讲的，如果是从地址空间的角度讲，应该是“tf之后”)
      又：T_SWITCH_USER的tf是没有最后8字节的
      */
      switchk2u.tf_esp = (uint32_t)tf + sizeof(struct trapframe) - 8;

      /*
      这个*((uint32_t *)tf - 1)是指向我们在__interrupt_entry里call
      interrupt_handler之前push进去的那个esp的 当时push
      esp的意图是，将tf作为interrupt_handler的参数传递（因为当时esp指向tf）

      call完interrupt_handler之后，__interrupt_entry的最后又把*((uint32_t *)tf -
      1)这个值读回esp寄存器去了 利用这一点，我们在此处修改*((uint32_t *)tf -
      1)，将它设置为switchk2u，
      实际上能使__interrupt_ret从switchk2u而不是tf恢复其他寄存器

      好吧，那你说，__interrupt_ret从switchk2u恢复完其他寄存器后又该怎么办呢，此时esp不就在错误的位置（switchk2u附近）了吗
      为了解决这个问题，请看下一行代码
      */
      *((uint32_t *)tf - 1) = (uint32_t)&switchk2u;

      /*
      最后一个问题，硬件进来（int）的时候push trapframe没有push
      ss和esp，那为什么返回（iret）的时候硬件自动就知道要多pop ss和esp了呢？
      原理是硬件检查了特权级是否发生了变化，如果有变化，就去多pop那两个东西出来。
      硬件怎么看特权级是否有变化呢？看cs寄存器里的某些bit就知道了

      所以这实际上也解释了为啥switchk2u是一个tf对象，而switchu2k只是一个tf指针，
      因为switchu2k的时候要转换特权级，所以硬件就在
      */
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
  case T_GPFLT: {
    panic(trapname(T_GPFLT));
  } break;
  case T_PGFLT: {
    print_pgfault(tf);
    panic(trapname(T_PGFLT));
  } break;
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
