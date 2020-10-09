CCPREFIX=i686-elf-

CC=$(CCPREFIX)gcc
AR=$(CCPREFIX)ar
LD=$(CCPREFIX)ld
OBJCOPY=$(CCPREFIX)objcopy

CFLAGS=-O0 -g -Wall -Wextra -ffreestanding -nostdinc
CVERSION=-std=gnu11
INCLUDE=-I../libno/include
LDFLAGS=-O0 -L../libno/user_build -lno

PROG_OBJS=\
hello_world/hello.exe

.PHONY: all clean crt
.SUFFIXES: .o .c .S

all: crt $(PROG_OBJS)

crt:
	cd ../kernel; make crt

%.exe: %.o
	$(CC) -T user.ld -o $@ $(CFLAGS) \
	../kernel/crt/crti.o ../kernel/crt/crtbegin.o \
	-nostdlib -L../libno/user_build -lno -static-libgcc -lgcc \
	../kernel/crt/crtend.o ../kernel/crt/crtn.o

%.o: %.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

clean:
	find . -name "*.o"  | xargs rm -f
	find . -name "*.a"  | xargs rm -f
	find . -name "*.exe"  | xargs rm -f
