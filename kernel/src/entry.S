#include <memlayout.h>
#include <mmu.h>

.align 4
.text
entry:
  # Turn on page size extension for 4MB pages
  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4
  # Set page directory
  movl    $(V2P(kernel_page_directory)), %eax
  movl    %eax, %cr3
  # Turn on paging.
  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  # Set up the stack pointer.
  movl $(P2V(KERNEL_STACK + KERNEL_STACK_SIZE - 1)), %esp
  
  # Jump to kentry(), and switch to executing at
  # high addresses. The indirect call is needed because
  # the assembler produces a PC-relative instruction
  # for a direct jump.
  mov $kentry, %eax
  jmp *%eax

spin:
  hlt
  jmp spin

.align 4
entry_flag:
    .long 8899174
    .word (entry_flag - entry)
