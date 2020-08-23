CCPREFIX=i686-elf-

CC=$(CCPREFIX)gcc
AR=$(CCPREFIX)ar
LD=$(CCPREFIX)ld
OBJCOPY=$(CCPREFIX)objcopy

CFLAGS=-O0 -g -Wall -Wextra -ffreestanding -nostdinc
CVERSION=-std=gnu11
LIBS=-nostdlib -L../libno -lno
INCLUDE=-Iinclude -I../libno/include -I../driver/include

KERNEL_OBJS=\
src/entry.o\
src/kernel.o\
src/kernel_pgd.o\
src/panic.o\
src/tty.o\
src/interrupt/interrupt_handler.o\
src/interrupt/idtvector.o\
src/interrupt/interrupt_entry.o

.PHONY: all clean
.SUFFIXES: .o .c .S

all: bootblock kernel

bootblock:
	$(CC) $(CFLAGS) -fno-pic -Os -g -nostdinc -Iinclude $(INCLUDE) -c arch/x86/bootloader.c -o arch/x86/bootc.o
	$(CC) $(CFLAGS) -fno-pic -Os -g -nostdinc -Iinclude $(INCLUDE) -c arch/x86/bootloader.S -o arch/x86/bootasm.o
	$(LD) -nostdlib -N -e start -Ttext 0x7C00 -o arch/x86/bootblock.o arch/x86/bootasm.o arch/x86/bootc.o
	$(OBJCOPY) -S -O binary -j .text arch/x86/bootblock.o bootblock
	../tools/sign bootblock bootblock
	cp arch/x86/bootblock.o unsigned_bootblock

kernel: $(KERNEL_OBJS) arch/x86/crti.o arch/x86/crtn.o arch/x86/crtbegin.o arch/x86/crtend.o 
	$(CC) -T kernel.ld -e 0xC0100000 -o kernel $(CFLAGS) \
	arch/x86/crti.o arch/x86/crtbegin.o \
	$(KERNEL_OBJS) -nostdlib -L../libno -lno -L../driver -ldriver \
	arch/x86/crtend.o arch/x86/crtn.o

arch/x86/crtbegin.o arch/x86/crtend.o:
	CPFROM=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$CPFROM" $@

src/interrupt/idtvector.o:
	../tools/idtgen src/interrupt/idtvector.S
	$(CC) -c src/interrupt/idtvector.S -o $@ $(CFLAGS) $(INCLUDE)

%.o: %.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

%.o: %.S
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE)

clean:
	rm -f *bootblock kernel
	rm -f src/interrupt/idtvector.S
	find . -name "*.o"  | xargs rm -f
	find . -name "*.a"  | xargs rm -f
