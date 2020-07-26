CCPREFIX=i686-elf-

CC=$(CCPREFIX)gcc
AR=$(CCPREFIX)ar
LD=$(CCPREFIX)ld
OBJCOPY=$(CCPREFIX)objcopy

CFLAGS=-O0 -g -Wall -Wextra -ffreestanding
CVERSION=-std=gnu11
LIBS=-nostdlib -L../libno -lno
INCLUDE=-Iinclude -I../libno/include

KERNEL_OBJS=\
src/kernel.o\
src/tty.o

.PHONY: all clean
.SUFFIXES: .o .c .S

all: bootblock kernel

bootblock:
	$(CC) $(CFLAGS) -fno-pic -Os -g -nostdinc -Iinclude -c arch/x86/bootloader.c -o arch/x86/bootc.o
	$(CC) $(CFLAGS) -fno-pic -Os -g -nostdinc -Iinclude -c arch/x86/bootloader.S -o arch/x86/bootasm.o
	$(LD) -N -e start -Ttext 0x7C00 -o arch/x86/bootblock.o arch/x86/bootasm.o arch/x86/bootc.o
	$(OBJCOPY) -S -O binary -j .text arch/x86/bootblock.o unsigned_bootblock
	../tools/sign unsigned_bootblock bootblock

kernel: $(KERNEL_OBJS)
	$(CC) -T kernel.ld -o kernel $(CFLAGS) $(KERNEL_OBJS) -nostdlib -L../libno -lno
	
%.o: %.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

%.S: %.c
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE)

clean:
	rm -f *bootblock kernel
	find . -name "*.o"  | xargs rm -f
	find . -name "*.a"  | xargs rm -f
