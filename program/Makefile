CCPREFIX=i686-elf-

CC=$(CCPREFIX)gcc
AR=$(CCPREFIX)ar
LD=$(CCPREFIX)ld
OBJCOPY=$(CCPREFIX)objcopy

CFLAGS=-O0 -DLIBNO_USER -g -Wall -Wextra -ffreestanding -nostdinc
CVERSION=-std=gnu11
INCLUDE=-I../libno/include

PROG_EXE=\
hello_world/hello.exe \
schd_test/main.exe \
count_down/main.exe \
task_test/main.exe \
abort/main.exe \
bad_access/main.exe \
user_read_input/main.exe \
signal_test/main.exe

.PHONY: all clean crt
.SUFFIXES: .o .c .S .exe

all: crt entry.o $(PROG_EXE)

crt:
	cd ../kernel; make crt

%.exe: %.o
	$(CC) -T user.ld -o $@ $(CFLAGS) \
	entry.o \
	$< \
	-nostdlib -L../libno/user_build -lno -static-libgcc -lgcc \

%.o: %.c
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

%.o: %.S
	$(CC) -c $< -o $@ $(CVERSION) $(CFLAGS) $(INCLUDE)

clean:
	find . -name "*.o"  | xargs rm -f
	find . -name "*.a"  | xargs rm -f
	find . -name "*.exe"  | xargs rm -f
