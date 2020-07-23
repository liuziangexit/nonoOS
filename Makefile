CC=i686-elf-gcc
ROOT=src
OUTPUT=build
CFLAGS=-O2 -g -Wall -Wextra -ffreestanding 
LIBS=-nostdlib -lno -lgcc

KERNELOBJS=\
$(ROOT)/kernel/multiboot/multiboot.o\
$(ROOT)/kernel/src/tty.o\
$(ROOT)/kernel/src/kernel.o\
$(ROOT)/kernel/multiboot/crti.o\
$(ROOT)/kernel/multiboot/crtn.o\

LINKLIST=\
$(ROOT)/kernel/multiboot/crti.o \
$(ROOT)/kernel/multiboot/crtbegin.o \
$(KERNEL_OBJS) \
$(LIBS) \
$(ROOT)/kernel/multiboot/crtend.o \
$(ROOT)/kernel/multiboot/crtn.o \

.PHONY: all clean
.SUFFIXES: .o .c .S

all: nonoOS.iso

nonoOS.iso: nonoOS.kernel

nonoOS.kernel: $(KERNELOBJS) $(ROOT)/kernel/multiboot/crtbegin.o $(ROOT)/kernel/multiboot/crtend.o
	$(CC) -T $(ROOT)/kernel/multiboot/linker.ld -o $@ $(CFLAGS) $(LINKLIST)

$(ROOT)/kernel/multiboot/crtbegin.o $(ROOT)/kernel/multiboot/crtend.o:
	CPFROM=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$CPFROM" $@

%.o: %.c
	$(CC) -c $< -o $@ -std=gnu11 -I$(ROOT)/kernel/include -I$(ROOT)/libno/include $(CFLAGS)

%.o: %.S
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -rf build
