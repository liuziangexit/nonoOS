CCPREFIX=i686-elf-

CC=$(CCPREFIX)gcc
AR=$(CCPREFIX)ar
LD=$(CCPREFIX)ld
OBJCOPY=$(CCPREFIX)objcopy

CFLAGS=-O0 -g -Wall -Wextra -ffreestanding -nostdinc
CVERSION=-std=gnu11
INCLUDE=-Iinclude -I../libno/include -I../driver/include

KERNEL_OBJS=\
src/entry.o\
src/kernel.o\
src/memory/kernel_pd.o\
src/memory/boot_stack.o\
src/memory/region.o\
src/panic.o\
src/ctl_char_handler.o\
src/tty.o\
src/memory/gdt.o\
src/memory/e820map.o\
src/memory/kmem_page_alloc.o\
src/memory/kmem_cache_alloc.o\
src/memory/kmem_alloc.o\
src/memory/kmem_init.o\
src/memory/bare_hashmap.o\
src/memory/virtual_memory.o\
src/memory/page_directory_debug.o\
src/memory/free_region.o\
src/interrupt/interrupt_handler.o\
src/interrupt/idtvector.o\
src/interrupt/interrupt_entry.o\
src/interrupt/clock.o\
src/debug/reg_info.o\
src/debug/kernel_size.o\
src/debug/print_e820.o\
src/task/task.o\
src/task/switch_to.o\
src/task/entry.o\
src/task/user_entry.o\
src/syscall/syscall.o\
src/sync/sync.o

.PHONY: all clean crt
.SUFFIXES: .o .c .S

all: bootblock kernel

bootblock:
	$(CC) $(CFLAGS) -fno-pic -Os -g -nostdinc -Iinclude $(INCLUDE) -c bootloader/bootloader.c -o bootloader/bootc.o
	$(CC) $(CFLAGS) -fno-pic -Os -g -nostdinc -Iinclude $(INCLUDE) -c bootloader/bootloader.S -o bootloader/bootasm.o
	$(LD) -nostdlib -N -e start -Ttext 0x7C00 -o bootloader/bootblock.o bootloader/bootasm.o bootloader/bootc.o
	$(OBJCOPY) -S -O binary -j .text bootloader/bootblock.o bootblock
	../tools/sign bootblock bootblock
	cp bootloader/bootblock.o unsigned_bootblock

kernel: $(KERNEL_OBJS) crt
	$(CC) -T kernel.ld -o kernel $(CFLAGS) \
	crt/crti.o crt/crtbegin.o \
	$(KERNEL_OBJS) \
	-nostdlib -L../libno/kernel_build -lno -L../driver -ldriver -static-libgcc -lgcc \
	crt/crtend.o crt/crtn.o \
	-Wl,-b -Wl,binary -Wl,../program/hello_world/hello.exe \
	-Wl,-b -Wl,binary -Wl,../program/schd_test/main.exe \
	-Wl,-b -Wl,binary -Wl,../program/count_down/main.exe
	

crt: crt/crti.o crt/crtn.o crt/crtbegin.o crt/crtend.o

crt/crtbegin.o crt/crtend.o:
	CPFROM=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$CPFROM" $@

src/interrupt/idtvector.o:
	../tools/idtgen src/interrupt/idtvector.S
	$(CC) -c src/interrupt/idtvector.S -o $@ $(CFLAGS) $(INCLUDE)

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
