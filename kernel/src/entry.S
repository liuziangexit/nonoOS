#include <memlayout.h>
#include <mmu.h>

#不知道为什么，在gdb里面b entry打不上断点。
#所以如果要调试这里的话，看一下bootloader.c里面载入entry的地址是什么，
#然后直接在那个地址上断点然后layout asm就好了，反正这个代码本来也就是汇编
.align 4
.text
entry:
flag:
  .long 0x4AB062DE
  # Turn on page size extension for 4MB pages
  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4
  # Set page directory
  movl    $(V2P(kernel_pd)), %eax
  movl    %eax, %cr3
  # Turn on paging.
  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0
  
  # calling GCC generated global constructor
  # TODO: find out what do they do and whether it's necessary to call it
  # call _init

  # Jump to kentry(), and switch to executing at
  # high addresses. The indirect call is needed because
  # the assembler produces a PC-relative instruction
  # for a direct jump.
  mov $kentry, %eax
  jmp *%eax

spin:
  hlt
  jmp spin
