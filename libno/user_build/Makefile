CCPREFIX=i686-elf-

CC=$(CCPREFIX)gcc
AR=$(CCPREFIX)ar
LD=$(CCPREFIX)LD

CFLAGS=-DLIBNO_USER -O0 -g -Wall -Wextra -ffreestanding
CVERSION=-std=gnu11
INCLUDE=-I../include -I../../kernel/include -I../../driver/include

OBJS=\
../src/avlhash.o\
../src/avlmini.o\
../src/atomic.o\
../src/stdout.o\
../src/stdin.o\
../src/string.o\
../src/spinlock.o\
../src/ring_buffer.o\
../src/abort_exit.o\
../src/itoa.o\
../src/ctype.o\
../src/round.o\
../src/malloc.o\
../src/syscall.o\
../src/list.o\
../src/task.o\
../src/unistd.o

.PHONY: all clean
.SUFFIXES: .o .c .S

all: libno.a

libno.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)
	find .. -name "*.o"  | xargs rm -f

%.o: %.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

src/round.o: src/round.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE) -w

clean:
	find .. -name "*.o"  | xargs rm -f
	find .. -name "*.a"  | xargs rm -f
