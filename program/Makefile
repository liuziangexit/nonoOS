CCPREFIX=i686-elf-

CC=$(CCPREFIX)gcc
AR=$(CCPREFIX)ar
LD=$(CCPREFIX)ld
OBJCOPY=$(CCPREFIX)objcopy

CFLAGS=-O0 -g -Wall -Wextra -ffreestanding -nostdinc
CVERSION=-std=gnu11
INCLUDE=-I../libno/include

PROG_OBJS=\
hello_world/main.o

.PHONY: all clean
.SUFFIXES: .o .c .S

all: $(PROG_OBJS)

%.o: %.c
	$(CC) -T user.ld -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

clean:
	find . -name "*.o"  | xargs rm -f
	find . -name "*.a"  | xargs rm -f
