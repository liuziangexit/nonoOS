#ifndef __KERNEL_GDT_H__
#define __KERNEL_GDT_H__

void gdt_init();
void load_esp0(uintptr_t esp0);

#endif