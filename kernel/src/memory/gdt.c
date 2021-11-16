#include <defs.h>
#include <gdt.h>
#include <mmu.h>
#include <x86.h>

static struct taskstate ts = {0};

static struct segdesc gdt[] = {
    SEG_NULL,
    [SEG_IDX_KCODE] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_IDX_KDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_IDX_UCODE] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_IDX_UDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_IDX_TSS] = SEG_NULL};

struct pseudodesc gdt_pd = {sizeof(gdt) - 1, (uintptr_t)gdt};

/* *
 * lgdt - load the global descriptor table register and reset the
 * data/code segement registers for kernel.
 * */
static inline void lgdt(struct pseudodesc *pd) {
  asm volatile("lgdt (%0)" ::"r"(pd));
  asm volatile("movw %%ax, %%gs" ::"a"(0));
  asm volatile("movw %%ax, %%fs" ::"a"(0));
  asm volatile("movw %%ax, %%es" ::"a"(KERNEL_DS));
  asm volatile("movw %%ax, %%ds" ::"a"(KERNEL_DS));
  asm volatile("movw %%ax, %%ss" ::"a"(KERNEL_DS));
  // reload cs
  asm volatile("ljmp %0, $1f\n 1:" ::"i"(KERNEL_CS));
}

unsigned char boot_tss_stack[1024];

void gdt_init() {
  load_esp0((uint32_t)boot_tss_stack + sizeof(boot_tss_stack));
  ts.ss0 = KERNEL_DS;

  // initialize the TSS filed of the gdt
  gdt[SEG_IDX_TSS] = SEGTSS(STS_T32A, (uint32_t)&ts, sizeof(ts), DPL_KERNEL);

  lgdt(&gdt_pd);
  ltr(SEG_TSS);
}

void load_esp0(uintptr_t esp0) { ts.esp0 = esp0; }

// 还没有task模块时候，系统用的tss栈
// 有了task之后，一个正在运行的用户线程如果遇到了中断，tss栈将会是utask::base.kstack
// 当一个异常在用户态发生时，系统需要push异常信息到栈上，tss就是用来储存这种异常栈的位置的
// 而tss只会被内核态代码修改，这就确保了安全

/*
这里有详细解释
https://stackoverflow.com/questions/35828347/how-does-a-software-based-context-switch-with-tss-work
First of all, the TSS is a historical wart. Once in a time (a.k.a: early
1980's), people at Intel tought that hardware context-switching, instead of
software context-switching, was a great idea. They were greatly wrong. Hardware
context-switching has several noticeable disadvantages, and, as it was never
implemented appropiately, had miserable performance. No sane OS even implemented
it due to all of that, plus the fact that it's even less portable than
segmentation. See the obscure corner of OSDevers for details.

Now, with respect to the Task State Segment. If any ever OS implemented hardware
context-switching, it's purpose is to represent a "task". It's possible to
represent both threads and processes as "tasks", but more often than not, in the
few code we have using hardware context-switching, it represents a simple
process. The TSS would hold stuff such as the task's general purpose register
contents, the control registers (CR0, CR2, CR3, and CR4; there's no CR1), CPU
flags and instruction pointer, etc...

However, in the real world, where software performs all context switches, we are
left with a 104-byte long structure which is (almost) useless. However, as we're
talking about Intel, it was never deprecated/removed, and OSes have to deal with
it.

The problem is actually pretty simple. Suppose you're running your typical foo()
function in your typical user-mode process. Suddenly, you, the user, press the
Windows/Meta/Super/however-you-call-it key in order to launch your mail client.
As a result, an interrupt request (IRQ) is sent from the keyboard into the
interrupt controller (either a 8259A PIC or a IOAPIC). Then, the interrupt
controller arranges things in order to trigger a CPU interrupt. The CPU enters
into privilege level 0, The registers are pushed, along with the interrupt
number, and kernel-mode code is invoked to handle the situation. Wait! Pushing
stuff? Where? On the stack, of course! But, where is the stack pointer taken
from in order to define a "stack"?

If you happened to use the user-mode stack pointer, bad things will happen, and
a giant security exploit would be available. What would happen if the stack
pointer pointed into an invalid address? It could happen. After all, strictly
speaking, the stack pointer is just another general purpose register, and
assembly programmers are known to use it that way for hardcoreness' sake.

An attempt to push stuff there would generate a CPU exception, nice! And, as
double faults (exceptions that occur while attempting to handle interrupts)
would yet again attempt to push over the invalid pointer, the worst nightmare of
an operating system becomes true: a triple fault. Have you ever seen your
computer suddenly reboot without any prior advice? That is a triple fault (or a
power failure). The OS has no change to handle a triple fault, it just reboots
everything.

Great, the system has rebooted. But, something worse could have happened. Had an
attacker purposefully written the address of a critical kernel variable (!), and
put the values that him would like written there in the right order, let the
greatest privilege elevation exploit reign as getting superuser privileges
becomes easier than ever! GDB, the kernel's configuration (found in
/proc/config.gz, and the GCC version the kernel was compiled with are more than
enough to do this.

Now, back to the TSS, it happens that the aforementioned structure contains the
values of the stack pointer and the stack segment register that are loaded upon
a interrupt while in privilege level 3 (user-mode). The kernel sets this to
point to a safe stack in kernel-land. As a result, there's a "kernel stack" per
thread in the system, and a TSS per each logical CPU in the system. Upon thread
switching, the kernel just changes these two variables in the right TSS. And no,
there can't be a single kernel stack per Logical CPU, because the kernel itself
may be preempted (most of the time).

I hope this has led some light on you!
*/
