CFLAGS=-O3 -Wall -Wextra -std=c11
LDFLAGS=-O3

.PHONY: all clean
.SUFFIXES: .o .c

all: sign

sign: sign.c
	$(CC) -c $< -o $@.o $(CFLAGS)
	$(CC) $@.o -o $@ $(LDFLAGS)

clean:
	rm -rf sign sign.o
