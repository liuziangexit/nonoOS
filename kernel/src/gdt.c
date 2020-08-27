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
  asm volatile("ljmp %0, $1f\n 1:\n" ::"i"(KERNEL_CS));
}

unsigned char stack0[1024];

void gdt_init() {
  ts.esp0 = stack0 + sizeof(stack0);
  ts.ss0 = KERNEL_DS;

  // initialize the TSS filed of the gdt
  gdt[SEG_IDX_TSS] = SEGTSS(STS_T32A, (uint32_t)&ts, sizeof(ts), DPL_KERNEL);

  lgdt(&gdt_pd);
  ltr(SEG_TSS);
}