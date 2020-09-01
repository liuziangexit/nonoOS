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
src/gdt.o\
src/kernel_pgd.o\
src/panic.o\
src/tty.o\
src/interrupt_handler.o\
src/idtvector.o\
src/interrupt_entry.o\
src/reg_info.o\
src/e820map.o\
src/ctl_char_handler.o\
src/kernel_size.o

.PHONY: all clean
.SUFFIXES: .o .c .S

all: bootblock kernel

bootblock:
	$(CC) $(CFLAGS) -fno-pic -Os -g -nostdinc -Iinclude $(INCLUDE) -c bootloader/bootloader.c -o bootloader/bootc.o
	$(CC) $(CFLAGS) -fno-pic -Os -g -nostdinc -Iinclude $(INCLUDE) -c bootloader/bootloader.S -o bootloader/bootasm.o
	$(LD) -nostdlib -N -e start -Ttext 0x7C00 -o bootloader/bootblock.o bootloader/bootasm.o bootloader/bootc.o
	$(OBJCOPY) -S -O binary -j .text bootloader/bootblock.o bootblock
	../tools/sign bootblock bootblock
	cp bootloader/bootblock.o unsigned_bootblock

kernel: $(KERNEL_OBJS) crt/crti.o crt/crtn.o crt/crtbegin.o crt/crtend.o 
	$(CC) -T kernel.ld -e 0xC0100000 -o kernel $(CFLAGS) \
	crt/crti.o crt/crtbegin.o \
	$(KERNEL_OBJS) -nostdlib -L../libno -lno -L../driver -ldriver \
	crt/crtend.o crt/crtn.o

crt/crtbegin.o crt/crtend.o:
	CPFROM=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$CPFROM" $@

src/idtvector.o:
	../tools/idtgen src/idtvector.S
	$(CC) -c src/idtvector.S -o $@ $(CFLAGS) $(INCLUDE)

src/interrupt_handler.o: src/interrupt_handler.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) -Wno-address-of-packed-member $(INCLUDE)

%.o: %.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

%.o: %.S
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE)

clean:
	rm -f *bootblock kernel
	rm -f src/interrupt/idtvector.S
	find . -name "*.o"  | xargs rm -f
	find . -name "*.a"  | xargs rm -f
