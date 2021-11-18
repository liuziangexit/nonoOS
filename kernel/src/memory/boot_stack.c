#include <assert.h>
#include <defs.h>
#include <memlayout.h>
#include <stdint.h>

/*
用来运行kmain的栈，主要用于调用各种init，等到创建task idle之后就没有用了
*/

uintptr_t boot_stack_paddr;

static uintptr_t in_page_offset(uintptr_t addr) {
  return addr % (4 * 1024 * 1024);
}

uintptr_t boot_stack_v2p(uintptr_t v) {
  assert((uintptr_t)ROUNDDOWN(v, 4 * 1024 * 1024) ==
         (uintptr_t)KERNEL_VIRTUAL_BOOT_STACK);
  return boot_stack_paddr + in_page_offset(v);
}
uintptr_t boot_stack_p2v(uintptr_t p) {
  assert((uintptr_t)ROUNDDOWN(p, 4 * 1024 * 1024) ==
         (uintptr_t)boot_stack_paddr);
  return KERNEL_VIRTUAL_BOOT_STACK + in_page_offset(p);
}