CCPREFIX=i686-elf-

CC=$(CCPREFIX)gcc
AR=$(CCPREFIX)ar
LD=$(CCPREFIX)LD

CFLAGS=-O0 -g -Wall -Wextra -ffreestanding
CVERSION=-std=gnu11
LIBS=-nostdlib -lgcc
INCLUDE=-Iinclude -I../kernel/include -I../driver/include

OBJS=\
src/stdout.o\
src/stdin.o\
src/string.o\
src/ring_buffer.o\
src/abort_exit.o\
src/itoa.o\
src/ctype.o

.PHONY: all clean
.SUFFIXES: .o .c .S

all: libno.a

libno.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

clean:
	find . -name "*.o"  | xargs rm -f
	find . -name "*.a"  | xargs rm -f
