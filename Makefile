.PHONY: all clean kernel libno tools driver prog
.SUFFIXES: .o .c

all: nonoOS.img

fs.img: tools
	tools/mkfs fs.img

nonoOS.img: kernel
	dd if=kernel/bootblock of=nonoOS.img conv=notrunc
	dd if=kernel/kernel of=nonoOS.img seek=1 conv=notrunc

kernel: tools driver libno prog
	cd kernel;	make

libno:
	cd libno/kernel_build;	make
	cd libno/user_build;	make

driver:
	cd driver;	make

tools:
	cd tools;	make

prog: kernel libno
	cd program;	make

clean:
	rm -f nonoOS.img
	rm -f fs.img
	cd kernel;	make clean
	cd tools;	make clean
	cd libno/kernel_build;	make clean
	cd libno/user_build;	make clean
	cd driver;  make clean
	cd program;  make clean
