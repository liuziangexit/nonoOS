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

kernel:
	movl $stack_top, %esp
	call _init
	call kentry
	call _fini
	cli
hang:
	hlt
	jmp hang
.size _start, . - _start
