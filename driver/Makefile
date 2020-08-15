CCPREFIX=i686-elf-

CC=$(CCPREFIX)gcc
AR=$(CCPREFIX)ar
LD=$(CCPREFIX)LD

CFLAGS=-O0 -g -Wall -Wextra -ffreestanding
CVERSION=-std=gnu11
LIBS=-nostdlib -lgcc
INCLUDE=-Iinclude -I../kernel/include

OBJS=\
src/cga.o

.PHONY: all clean
.SUFFIXES: .o .c .S

all: libdriver.a

libdriver.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

clean:
	find . -name "*.o"  | xargs rm -f
	find . -name "*.a"  | xargs rm -f
