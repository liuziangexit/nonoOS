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

//还没有task模块时候，系统用的tss栈
//有了task之后，一个正在运行的用户线程如果遇到了中断，tss栈将会是utask::base.kstack
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