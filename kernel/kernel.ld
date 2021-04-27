/* Simple linker script for the JOS kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

SECTIONS {
	/* Load the kernel at this address: "." means the current address */
	. = 0xC0100000;

	PROVIDE(program_begin = .);
	
	PROVIDE(text_begin = .);
	.text : {
		*(.text .stub .text.* .gnu.linkonce.t.*)
	}
	PROVIDE(text_end = .);
	
	PROVIDE(rodata_begin = .);
	.rodata : {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}
	PROVIDE(rodata_end = .);

	/* Adjust the address for the data segment to the next page */
	. = ALIGN(0x1000);
	
	PROVIDE(data_begin = .);
	.data : {
		*(.data)
	}
	PROVIDE(data_end = .);

	PROVIDE(bss_begin = .);
	.bss : {
		*(.bss)
	}
	PROVIDE(bss_end = .);

	PROVIDE(program_end = .);
}
