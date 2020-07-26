.PHONY: all clean kernel libno tools
.SUFFIXES: .o .c

all: nonoOS.img

nonoOS.img: kernel
	dd if=/dev/zero of=nonoOS.img count=10000
	dd if=kernel/bootblock of=nonoOS.img conv=notrunc
	dd if=kernel/kernel of=nonoOS.img seek=1 conv=notrunc

kernel: tools libno
	cd kernel;	make

libno:
	cd libno;	make

tools:
	cd tools;	make

clean:
	rm -f nonoOS.img
	cd kernel;	make clean
	cd tools;	make clean
	cd libno;	make clean
