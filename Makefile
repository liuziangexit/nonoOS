CC=i686-elf-gcc
AR=i686-elf-ar
MKRESCUE=i386-elf-grub-mkrescue

ROOT=src
CFLAGS=-O2 -g -Wall -Wextra -ffreestanding 
LIBS=-nostdlib -lno -lgcc

KERNELOBJS=\
$(ROOT)/kernel/arch/x86/boot.o\
$(ROOT)/kernel/src/tty.o\
$(ROOT)/kernel/src/kernel.o

LIBNOOBJS=\
$(ROOT)/libno/src/abort.o\
$(ROOT)/libno/src/printf.o\
$(ROOT)/libno/src/putchar.o\
$(ROOT)/libno/src/puts.o\
$(ROOT)/libno/src/string.o

LINKLIST=\
$(ROOT)/kernel/arch/x86/crti.o \
$(ROOT)/kernel/arch/x86/crtbegin.o \
$(KERNELOBJS) \
$(LIBS) \
$(ROOT)/kernel/arch/x86/crtend.o \
$(ROOT)/kernel/arch/x86/crtn.o \

.PHONY: all clean
.SUFFIXES: .o .c .S

all: $(ROOT)/kernel/nonoOS.kernel

$(ROOT)/libno/libno.a: $(LIBNOOBJS)
	$(AR) rcs $@ $(LIBNOOBJS)

$(ROOT)/kernel/nonoOS.kernel: $(ROOT)/libno/libno.a $(KERNELOBJS) $(ROOT)/kernel/arch/x86/crti.o $(ROOT)/kernel/arch/x86/crtn.o $(ROOT)/kernel/arch/x86/crtbegin.o $(ROOT)/kernel/arch/x86/crtend.o
	$(CC) -T $(ROOT)/kernel/arch/x86/linker.ld -o $@ $(CFLAGS) -L$(ROOT)/libno $(LINKLIST) 

$(ROOT)/kernel/arch/x86/crtbegin.o $(ROOT)/kernel/arch/x86/crtend.o:
	CPFROM=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$CPFROM" $@

%.o: %.c
	$(CC) -c $< -o $@ -std=gnu11 -I$(ROOT)/kernel/include -I$(ROOT)/libno/include $(CFLAGS)

%.o: %.S
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(ROOT)/kernel/nonoOS.kernel
	find $(ROOT) -name "*.o"  | xargs rm -f
	find $(ROOT) -name "*.a"  | xargs rm -f
