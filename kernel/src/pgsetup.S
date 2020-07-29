#include <memlayout.h>
#include <mmu.h>

# Entering xv6 on boot processor, with paging off.
.align 4
.text
.globl entry
entry:
  # Turn on page size extension for 4Mbyte pages
  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4
  # Set page directory(呱呱呱)
  movl    $(V2P(page_directory)), %eax
  movl    %eax, %cr3
  # Turn on paging.
  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  # Set up the stack pointer.
  movl $(stack + KERNEL_STACK_SIZE), %esp

  # Jump to main(), and switch to executing at
  # high addresses. The indirect call is needed because
  # the assembler produces a PC-relative instruction
  # for a direct jump.
  mov $kentry, %eax
  jmp *%eax

.comm stack, KERNEL_STACK_SIZE
