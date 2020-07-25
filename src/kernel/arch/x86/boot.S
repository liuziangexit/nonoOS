#include "segment.h"
#include "mmu.h"

# declare constants for the multiboot header.
.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set FLAGS,    ALIGN | MEMINFO  # this is the Multiboot 'flag' field

# multiboot header
.section .multiboot
.align 4
.long 0x1BADB002 # multiboot magic number
.long FLAGS
.long -(0x1BADB002 + FLAGS) # checksum of above, to prove we are multiboot

# bare C stack
.section .bss
.align 16
stack_bottom:
.skip 16 * 1024
stack_top:

# kernel entry
.section .text
.global _start
.type _start, @function
_start:
	cli
seta20.1:
	inb     $0x64,%al               # Wait for not busy
	testb   $0x2,%al
	jnz     seta20.1
	
	movb    $0xd1,%al               # 0xd1 -> port 0x64
	outb    %al,$0x64

seta20.2:
	inb     $0x64,%al               # Wait for not busy
	testb   $0x2,%al
	jnz     seta20.2
	
	movb    $0xdf,%al               # 0xdf -> port 0x60
	outb    %al,$0x60

setgdt:
	lgdt    gdtdesc
	movl    %cr0, %eax
	orl     $CR0_PE, %eax
	movl    %eax, %cr0
	# Complete the transition to 32-bit protected mode by using a long jmp
	# to reload %cs and %eip.  The segment descriptors are set up with no
	# translation, so that the mapping is still the identity mapping.
	ljmp    $(SEG_KCODE<<3), $kernel

kernel:
	movl $stack_top, %esp
	call _init
	call kentry
	call _fini
	cli

spin:
	hlt
	jmp spin

# Bootstrap GDT
.p2align 2                                # force 4 byte alignment
gdt:
  SEG_NULLASM                             # null seg
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # code seg
  SEG_ASM(STA_RW, 0x0, 0xffffffff)         # data seg

gdtdesc:
  .word   (gdtdesc - gdt - 1)             # sizeof(gdt) - 1
  .long   gdt         

.size _start, . - _start
